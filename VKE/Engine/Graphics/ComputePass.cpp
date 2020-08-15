#include "ComputePass.h"
#include "Descriptors/Descriptor_Buffer.h"

namespace VKE
{
	bool FComputePass::SComputePipelineRequired = false;

	bool FComputePass::needSynchronization() const
	{
		assert(pMainDevice);
		if (pMainDevice)
		{
			return pMainDevice->QueueFamilyIndices.computeFamily != pMainDevice->QueueFamilyIndices.graphicFamily;
		}
		return false;
	}

	void FComputePass::init(FMainDevice* const iMainDevice)
	{
		if (!SComputePipelineRequired)
		{
			SComputePipelineRequired = true;
		}
		pMainDevice = iMainDevice;
		ComputeDescriptorSet.pMainDevice = pMainDevice;
		// 1. Get compute queue and queue family
		vkGetDeviceQueue(pMainDevice->LD, pMainDevice->QueueFamilyIndices.computeFamily, 0, &ComputeQueue);
		// . Create compute command pool
		createCommandPool();
		// . Create command buffer
		createCommandBuffer();
		
		// . Setup Emitter
		Emitter.Transform.SetPosition(glm::vec3(0.0, 1.0, 1.0));
		Emitter.EmitterData.Radius = 0.2f;
		Emitter.EmitterData.Angle = glm::radians(45.f);
		Emitter.EmitterData.StartSpeedMin = 1.0f;
		Emitter.EmitterData.StartSpeedMax = 1.0f;
		Emitter.EmitterData.StartDelayRangeMin = 0.0f;
		Emitter.EmitterData.StartDelayRangeMax = 2.0f;
		Emitter.EmitterData.LifeTimeRangeMin = 1.0f;
		Emitter.EmitterData.LifeTimeRangeMax = 1.0f;

		// . create storage buffer and uniform buffer
		createStorageBuffer();
		createUniformBuffer();
		// . set up descriptor set related
		prepareDescriptors();
		// . Create semaphores and fences
		createSynchronization();
		// . Create compute pipeline
		createComputePipeline();
		// . Need to signal in init stage because the graphic pass is running before the compute pass, so making sure no need to wait at for the compute pass in the first render
		// Signal the semaphore with an empty queue (no wait, no command buffer, no fence to signal)
		{
			VkSubmitInfo SubmitInfo = {};
			SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			SubmitInfo.signalSemaphoreCount = 1;
			SubmitInfo.pSignalSemaphores = &OnComputeFinished;

			VkResult Result = vkQueueSubmit(ComputeQueue, 1, &SubmitInfo, VK_NULL_HANDLE);
			RESULT_CHECK(Result, "Fail to submit initial compute queue");
			// Wait for the queue to finish
			Result = vkQueueWaitIdle(ComputeQueue);
			RESULT_CHECK(Result, "Fail to wait the queue to finish");
		}

		// . Record command lines
		recordComputeCommands();

		// If graphics and compute queue family indices differ, acquire and immediately release the storage buffer, so that the initial acquire from the graphics command buffers are matched up properly
		if (needSynchronization())
		{
			const int& GraphicFamilyIndex = pMainDevice->QueueFamilyIndices.graphicFamily;
			const int& ComputeFamilyIndex = pMainDevice->QueueFamilyIndices.computeFamily;
			const cBuffer& StorageBuffer = ComputeDescriptorSet.GetDescriptorAt<cDescriptor_Buffer>(0)->GetBuffer();

			// Create a transient command buffer for setting up the initial buffer transfer state
			VkCommandBuffer TransferCommandBuffer = BeginCommandBuffer(pMainDevice->LD, ComputeCommandPool);

			VkBufferMemoryBarrier AcquireBufferBarrier = {};
			AcquireBufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			AcquireBufferBarrier.srcAccessMask = 0;
			AcquireBufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			AcquireBufferBarrier.srcQueueFamilyIndex = GraphicFamilyIndex;
			AcquireBufferBarrier.dstQueueFamilyIndex = ComputeFamilyIndex;
			AcquireBufferBarrier.buffer = StorageBuffer.GetvkBuffer();
			AcquireBufferBarrier.offset = 0;
			AcquireBufferBarrier.size = StorageBuffer.BufferSize();

			vkCmdPipelineBarrier(TransferCommandBuffer,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,
				0, nullptr,
				1, &AcquireBufferBarrier,
				0, nullptr);

			VkBufferMemoryBarrier ReleaseBufferBarrier = {};
			ReleaseBufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			ReleaseBufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			ReleaseBufferBarrier.dstAccessMask = 0;
			ReleaseBufferBarrier.srcQueueFamilyIndex = ComputeFamilyIndex;
			ReleaseBufferBarrier.dstQueueFamilyIndex = GraphicFamilyIndex;
			ReleaseBufferBarrier.buffer = StorageBuffer.GetvkBuffer();
			ReleaseBufferBarrier.offset = 0;
			ReleaseBufferBarrier.size = StorageBuffer.BufferSize();
			vkCmdPipelineBarrier(
				TransferCommandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				0,
				0, nullptr,
				1, &ReleaseBufferBarrier,
				0, nullptr);

			EndCommandBuffer(TransferCommandBuffer, pMainDevice->LD, ComputeQueue, ComputeCommandPool);
		}
	}

	void FComputePass::cleanUp()
	{
		// wait until the device is not doing anything (nothing on any queue)
		vkDeviceWaitIdle(pMainDevice->LD);

		vkDestroySemaphore(pMainDevice->LD, OnComputeFinished, nullptr);

		vkDestroyCommandPool(pMainDevice->LD, ComputeCommandPool, nullptr);

		vkDestroyPipeline(pMainDevice->LD, ComputePipeline, nullptr);
		vkDestroyPipelineLayout(pMainDevice->LD, ComputePipelineLayout, nullptr);

		ComputeDescriptorSet.cleanUp();
		vkDestroyDescriptorPool(pMainDevice->LD, DescriptorPool, nullptr);
	}

	void FComputePass::recordComputeCommands()
	{
		VkCommandBufferBeginInfo BufferBeginInfo = {};
		BufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		//BufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		// Begin command buffer
		VkResult Result = vkBeginCommandBuffer(CommandBuffer, &BufferBeginInfo);
		RESULT_CHECK(Result, "Fail to start recording a compute command buffer");

		// Particle Movement

		const int& GraphicFamilyIndex = pMainDevice->QueueFamilyIndices.graphicFamily;
		const int& ComputeFamilyIndex = pMainDevice->QueueFamilyIndices.computeFamily;
		const cBuffer& StorageBuffer = ComputeDescriptorSet.GetDescriptorAt<cDescriptor_Buffer>(0)->GetBuffer();
		// Add memory barrier to ensure that the (graphics) vertex shader has fetched attributes before compute starts to write to the buffer
		if (needSynchronization())
		{
			VkBufferMemoryBarrier BufferBarrier = {};
			BufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			BufferBarrier.srcAccessMask = 0;
			// Compute shader will write data to the storage buffer
			BufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			// Transfer ownership from graphic queue to compute queue
			BufferBarrier.srcQueueFamilyIndex = GraphicFamilyIndex;
			BufferBarrier.dstQueueFamilyIndex = ComputeFamilyIndex;
			BufferBarrier.buffer = StorageBuffer.GetvkBuffer();
			BufferBarrier.offset = 0;
			BufferBarrier.size = StorageBuffer.BufferSize();

			// Block the compute shader until vertex shader finished reading the storage buffer
			vkCmdPipelineBarrier(CommandBuffer,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,							// No Dependency flag
				0, nullptr,					// Not a Memory barrier
				1, &BufferBarrier,			// A Buffer memory Barrier
				0, nullptr);				// Not a Image memory barrier
		}

		// Dispatch the compute job
		vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ComputePipeline);
		vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ComputePipelineLayout, 0, 1, &ComputeDescriptorSet.GetDescriptorSet(), 0, 0);
		vkCmdDispatch(CommandBuffer, Particle_Count / Dispatch_Size_X, 1, 1);

		// Add barrier to ensure that compute shader has finished writing to the buffer
		// Without this the (rendering) vertex shader may display incomplete results (partial data from last frame)
		if (needSynchronization())
		{
			VkBufferMemoryBarrier BufferBarrier = {};
			BufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			BufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			BufferBarrier.dstAccessMask = 0;
			// Transfer owner ship from compute queue to graphic queue
			BufferBarrier.srcQueueFamilyIndex = ComputeFamilyIndex;
			BufferBarrier.dstQueueFamilyIndex = GraphicFamilyIndex;
			BufferBarrier.buffer = StorageBuffer.GetvkBuffer();
			BufferBarrier.offset = 0;
			BufferBarrier.size = StorageBuffer.BufferSize();

			// Block the vertex input stage until compute shader has finished
			vkCmdPipelineBarrier(CommandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				0,
				0, nullptr,
				1, &BufferBarrier,
				0, nullptr);
		}
		Result = vkEndCommandBuffer(CommandBuffer);
		RESULT_CHECK(Result, "Fail to stop recording a compute command buffer");

	}

	const VKE::cBuffer& FComputePass::GetStorageBuffer()
	{
		return ComputeDescriptorSet.GetDescriptorAt<cDescriptor_Buffer>(0)->GetBuffer();
	}

	void FComputePass::undateUniformBuffer()
	{
		ComputeDescriptorSet.GetDescriptorAt<cDescriptor_Buffer>(1)->UpdateBufferData(&ParticleSupportData);
	}

	void FComputePass::createStorageBuffer()
	{
		
		// Initialize the particle data
		for (size_t i = 0; i < Particle_Count; ++i)
		{
			Emitter.NextParticle(Particles[i]);
		}
		// Create storage buffer
		VkDeviceSize StorageBufferSize = sizeof(BufferFormats::FParticle) * Particle_Count;
		cBuffer StagingBuffer;

		if (!StagingBuffer.CreateBufferAndAllocateMemory(pMainDevice->PD, pMainDevice->LD, StorageBufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
		{
			return;
		}

		// Map particle data to the staging buffer
		void * pData = nullptr;
		vkMapMemory(pMainDevice->LD, StagingBuffer.GetMemory(), 0, StorageBufferSize, 0, &pData);
		memcpy(pData, Particles, static_cast<size_t>(StorageBufferSize));
		vkUnmapMemory(pMainDevice->LD, StagingBuffer.GetMemory());

		// Create storage buffer, Binding = 0
		ComputeDescriptorSet.CreateStorageBufferDescriptor(StorageBufferSize, 1, VK_SHADER_STAGE_COMPUTE_BIT,
			// 1. As transfer destination from staging buffer, 2. As storage buffer storing particle data in compute shader, 3. As vertex data in vertex shader
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			// Local hosted buffer, need get data from staging buffer 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		// Allocate the transfer command buffer
		VkCommandBuffer TransferCommandBuffer = BeginCommandBuffer(pMainDevice->LD, pMainDevice->GraphicsCommandPool);

		// Region of data to copy from and to, allows copy multiple regions of data
		VkBufferCopy BufferCopyRegion = {};
		BufferCopyRegion.srcOffset = 0;
		BufferCopyRegion.dstOffset = 0;
		BufferCopyRegion.size = StorageBufferSize;

		// Command to copy src buffer to dst buffer
		const cBuffer& StorageBuffer = ComputeDescriptorSet.GetDescriptorAt<cDescriptor_Buffer>(0)->GetBuffer();
		vkCmdCopyBuffer(TransferCommandBuffer, StagingBuffer.GetvkBuffer(), StorageBuffer.GetvkBuffer(), 1, &BufferCopyRegion);
		// Setup a barrier when compute queue is not the same as the graphic queue
		if (needSynchronization())
		{
			VkBufferMemoryBarrier Barrier = {};
			Barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			Barrier.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
			Barrier.dstAccessMask = 0;
			Barrier.srcQueueFamilyIndex = pMainDevice->QueueFamilyIndices.graphicFamily;
			Barrier.dstQueueFamilyIndex = pMainDevice->QueueFamilyIndices.computeFamily;
			Barrier.buffer = StorageBuffer.GetvkBuffer();
			Barrier.offset = 0;
			Barrier.size = StorageBufferSize;

			vkCmdPipelineBarrier(TransferCommandBuffer,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,			// source stage
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,	// destination stage
				0, nullptr,			// memory
				1, &Barrier,		// Buffer
				0, nullptr);		// Image
		}
		// Stop the command and submit it to the queue, wait until it finish execution
		EndCommandBuffer(TransferCommandBuffer, pMainDevice->LD, pMainDevice->graphicQueue, pMainDevice->GraphicsCommandPool);

		// Clean up staging buffer parts
		StagingBuffer.cleanUp();
	}

	void FComputePass::createUniformBuffer()
	{
		// Binding = 1, dt, gravity
		ComputeDescriptorSet.CreateBufferDescriptor(sizeof(BufferFormats::FParticleSupportData), 1, VK_SHADER_STAGE_COMPUTE_BIT);

		// Setup initial data
		ParticleSupportData.dt = 0.0005f;
		ParticleSupportData.useGravity = false;
		ComputeDescriptorSet.GetDescriptorAt<cDescriptor_Buffer>(1)->UpdateBufferData(&ParticleSupportData);

		// Binding = 2, emitter data
		ComputeDescriptorSet.CreateBufferDescriptor(sizeof(BufferFormats::FConeEmitter), 1, VK_SHADER_STAGE_COMPUTE_BIT);
		ComputeDescriptorSet.GetDescriptorAt<cDescriptor_Buffer>(2)->UpdateBufferData(&Emitter.EmitterData);
	}

	void FComputePass::prepareDescriptors()
	{
		// 1. Create descriptor pools
		//-------------------------------------------------------
		{
			const uint32_t DescriptorTypeCount = 2;
			VkDescriptorPoolSize PoolSize[DescriptorTypeCount] = {};
			PoolSize[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			PoolSize[0].descriptorCount = 1;
			PoolSize[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			PoolSize[1].descriptorCount = 2;

			VkDescriptorPoolCreateInfo PoolCreateInfo = {};
			PoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			PoolCreateInfo.maxSets = 3;
			PoolCreateInfo.poolSizeCount = DescriptorTypeCount;
			PoolCreateInfo.pPoolSizes = PoolSize;

			VkResult Result = vkCreateDescriptorPool(pMainDevice->LD, &PoolCreateInfo, nullptr, &DescriptorPool);
			RESULT_CHECK(Result, "Failed to create a compute Descriptor Pool");
		}

		ComputeDescriptorSet.CreateDescriptorSetLayout(ComputePass);
		ComputeDescriptorSet.AllocateDescriptorSet(DescriptorPool);
		ComputeDescriptorSet.BindDescriptorWithSet();

	}

	void FComputePass::createComputePipeline()
	{
		// 1. Create pipeline layout according to the descriptor set layout
		VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = {};

		PipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		PipelineLayoutCreateInfo.setLayoutCount = 1;
		PipelineLayoutCreateInfo.pSetLayouts = &ComputeDescriptorSet.GetDescriptorSetLayout();
		PipelineLayoutCreateInfo.pushConstantRangeCount = 0;
		PipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

		VkResult Result = vkCreatePipelineLayout(pMainDevice->LD, &PipelineLayoutCreateInfo, nullptr, &ComputePipelineLayout);
		RESULT_CHECK(Result, "Fail to craete comptue pipeline layout.");

		// 2. Load shader
		FShaderModuleScopeGuard ComputeShaderModule;
		std::vector<char> FragShaderCode = FileIO::ReadFile("Content/Shaders/particle/particle.comp.spv");
		ComputeShaderModule.CreateShaderModule(pMainDevice->LD, FragShaderCode);

		// 3. Create Shader stage
		VkPipelineShaderStageCreateInfo ComputeShaderStage = {};
		ComputeShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		ComputeShaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		ComputeShaderStage.module = ComputeShaderModule.ShaderModule;
		ComputeShaderStage.pName = "main";

		// 4. Create the compute pipeline
		VkComputePipelineCreateInfo ComputePipelineCreateInfo = {};

		ComputePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		ComputePipelineCreateInfo.flags = 0;
		ComputePipelineCreateInfo.layout = ComputePipelineLayout;
		ComputePipelineCreateInfo.stage = ComputeShaderStage;
		ComputePipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		ComputePipelineCreateInfo.basePipelineIndex = -1;

		Result = vkCreateComputePipelines(pMainDevice->LD, VK_NULL_HANDLE, 1, &ComputePipelineCreateInfo, nullptr, &ComputePipeline);
	}

	void FComputePass::createCommandPool()
	{
		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = pMainDevice->QueueFamilyIndices.computeFamily;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		auto Result = vkCreateCommandPool(pMainDevice->LD, &cmdPoolInfo, nullptr, &ComputeCommandPool);
		RESULT_CHECK(Result, "Fail to create Compute Command Pool\n");
	}

	void FComputePass::createCommandBuffer()
	{
		VkCommandBufferAllocateInfo cbAllocInfo = {};
		cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cbAllocInfo.commandPool = ComputeCommandPool;
		cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		cbAllocInfo.commandBufferCount = 1;

		VkResult Result = vkAllocateCommandBuffers(pMainDevice->LD, &cbAllocInfo, &CommandBuffer);
		RESULT_CHECK(Result, "Fail to allocate compute command buffers.");
	}

	void FComputePass::createSynchronization()
	{
		// 1. Create semaphore
		{
			VkSemaphoreCreateInfo SemaphoreCreateInfo = {};
			SemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			VkResult Result = vkCreateSemaphore(pMainDevice->LD, &SemaphoreCreateInfo, nullptr, &OnComputeFinished);
			RESULT_CHECK(Result, "Fail to create OnComputeFinish Semaphore");
		}
	}
}