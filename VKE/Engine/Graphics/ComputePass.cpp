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
			return pMainDevice->NeedSynchronization();
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
		
		// 1. Get compute queue and queue family
		vkGetDeviceQueue(pMainDevice->LD, pMainDevice->QueueFamilyIndices.computeFamily, 0, &ComputeQueue);
		// . Create compute command pool
		createCommandPool();
		// . Create command buffer
		createCommandBuffer();
		
		// . Setup Emitter

		Emitters.resize(3);
		Emitters[0].TextureToUse = FTexture::Get(1);
		Emitters[0].Transform.SetPosition(glm::vec3(0.0, 0.1, 0.0));
		Emitters[0].EmitterData.Radius = 0.15f;
		Emitters[0].EmitterData.Angle = glm::radians(10.f);
		Emitters[0].EmitterData.StartSpeedMin = 1.0f;
		Emitters[0].EmitterData.StartSpeedMax = 1.5f;
		Emitters[0].EmitterData.StartDelayRangeMin = 0.0f;
		Emitters[0].EmitterData.StartDelayRangeMax = 2.0f;
		Emitters[0].EmitterData.LifeTimeRangeMin = 1.2f;
		Emitters[0].EmitterData.LifeTimeRangeMax = 1.2f;
		Emitters[0].EmitterData.StartColor = glm::vec4(1.0f);
		Emitters[0].EmitterData.ColorOverLifeTimeStart = glm::vec4(1.0, 0.76f, 0.125, 1.0f);
		Emitters[0].EmitterData.ColorOverLifeTimeEnd = glm::vec4(1.0, 0.10f, 0.0, 0.0f);
		Emitters[0].EmitterData.StartSizeMin = 0.1f;
		Emitters[0].EmitterData.StartSizeMax = 0.2f;
		Emitters[0].EmitterData.NoiseMin = -30.0f;
		Emitters[0].EmitterData.NoiseMax = 30.0f;
		Emitters[0].EmitterData.bEnableSubTexture = false;
		Emitters[0].EmitterData.TileWidth = 1;

		Emitters[1].TextureToUse = FTexture::Get(1);
		Emitters[1].Transform.SetPosition(glm::vec3(0.0, -0.1, 0.0));
		Emitters[1].EmitterData.Radius = 0.1f;
		Emitters[1].EmitterData.Angle = glm::radians(5.f);
		Emitters[1].EmitterData.StartSpeedMin = 1.0f;
		Emitters[1].EmitterData.StartSpeedMax = 1.5f;
		Emitters[1].EmitterData.StartDelayRangeMin = 0.0f;
		Emitters[1].EmitterData.StartDelayRangeMax = 1.0f;
		Emitters[1].EmitterData.LifeTimeRangeMin = 0.7f;
		Emitters[1].EmitterData.LifeTimeRangeMax = 0.8f;
		Emitters[1].EmitterData.StartColor = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
		Emitters[1].EmitterData.ColorOverLifeTimeStart = glm::vec4(1.0, 0.76f, 0.125, 1.0f) ;
		Emitters[1].EmitterData.ColorOverLifeTimeEnd = glm::vec4(1.0, 0.10f, 0.0, 0.0f);
		Emitters[1].EmitterData.StartSizeMin = 0.7f;
		Emitters[1].EmitterData.StartSizeMax = 0.8f;
		Emitters[1].EmitterData.NoiseMin = -5.0f;
		Emitters[1].EmitterData.NoiseMax = 5.0f;
		Emitters[1].EmitterData.bEnableSubTexture = false;
		Emitters[1].EmitterData.TileWidth = 1;

		Emitters[2].TextureToUse = FTexture::Get(2);	// Fire * 4
		Emitters[2].Transform.SetPosition(glm::vec3(0.0, 0.1, 0.0));
		Emitters[2].EmitterData.Radius = 0.1f;
		Emitters[2].EmitterData.Angle = glm::radians(5.f);
		Emitters[2].EmitterData.StartSpeedMin = 1.0f;
		Emitters[2].EmitterData.StartSpeedMax = 1.5f;
		Emitters[2].EmitterData.StartDelayRangeMin = 0.0f;
		Emitters[2].EmitterData.StartDelayRangeMax = 1.0f;
		Emitters[2].EmitterData.LifeTimeRangeMin = 0.7f;
		Emitters[2].EmitterData.LifeTimeRangeMax = 0.8f;
		Emitters[2].EmitterData.StartColor = glm::vec4(1.0f, 0.86f, 0.74f, 1.0f) * 0.8f;
		Emitters[2].EmitterData.ColorOverLifeTimeStart = glm::vec4(1.0, 0.76f, 0.125, 1.0f);
		Emitters[2].EmitterData.ColorOverLifeTimeEnd = glm::vec4(1.0, 0.10f, 0.0, 0.0f);
		Emitters[2].EmitterData.StartSizeMin = 0.35f;
		Emitters[2].EmitterData.StartSizeMax = 0.5f;
		Emitters[2].EmitterData.NoiseMin = -5.0f;
		Emitters[2].EmitterData.NoiseMax = 5.0f;
		Emitters[2].EmitterData.StartRotationMin = -45.f;
		Emitters[2].EmitterData.StartRotationMax = 45.f;
		Emitters[2].EmitterData.bEnableSubTexture = true;
		Emitters[2].EmitterData.TileWidth = 2;

		for (size_t i = 0; i < Emitters.size(); ++i)
		{
			// . initialize data, create storage buffer and uniform buffer
			Emitters[i].init(iMainDevice);
			Emitters[i].Transform.Update();
		}
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

		const uint32_t EmitterCount = Emitters.size();
		// If graphics and compute queue family indices differ, acquire and immediately release the storage buffer, so that the initial acquire from the graphics command buffers are matched up properly
		if (needSynchronization())
		{
			// Create a transient command buffer for setting up the initial buffer transfer state
			VkCommandBuffer TransferCommandBuffer = BeginCommandBuffer(pMainDevice->LD, ComputeCommandPool);
			
			std::vector<VkBufferMemoryBarrier> BufferBarriers(EmitterCount);
			for (uint32_t i = 0; i < EmitterCount; ++i)
			{
				BufferBarriers[i] = Emitters[i].ComputeOwnBarrier(0, VK_ACCESS_SHADER_WRITE_BIT);
			}

			vkCmdPipelineBarrier(TransferCommandBuffer,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,
				0, nullptr,
				EmitterCount, BufferBarriers.data(),
				0, nullptr);
			
			for (uint32_t i = 0; i < EmitterCount; ++i)
			{
				BufferBarriers[i] = Emitters[i].GraphicOwnBarrier(VK_ACCESS_SHADER_WRITE_BIT, 0);
			}

			vkCmdPipelineBarrier(
				TransferCommandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				0,
				0, nullptr,
				EmitterCount, BufferBarriers.data(),
				0, nullptr);

			EndCommandBuffer(TransferCommandBuffer, pMainDevice->LD, ComputeQueue, ComputeCommandPool);
		}
	}

	void FComputePass::cleanUp()
	{
		// wait until the device is not doing anything (nothing on any queue)
		vkDeviceWaitIdle(pMainDevice->LD);

		vkDestroySemaphore(pMainDevice->LD, OnComputeFinished, nullptr);

		cleanupSwapChain();

		vkDestroyCommandPool(pMainDevice->LD, ComputeCommandPool, nullptr);
		
		for (cEmitter& emitter : Emitters)
		{
			emitter.cleanUp();
		}
		Emitters.clear();

		vkDestroyDescriptorPool(pMainDevice->LD, DescriptorPool, nullptr);
	}

	void FComputePass::recordComputeCommands()
	{
		const uint32_t EmitterCount = Emitters.size();
		VkCommandBufferBeginInfo BufferBeginInfo = {};
		BufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		//BufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		// Begin command buffer
		VkResult Result = vkBeginCommandBuffer(CommandBuffer, &BufferBeginInfo);
		RESULT_CHECK(Result, "Fail to start recording a compute command buffer");

		// Particle Movement

		// Add memory barrier to ensure that the (graphics) vertex shader has fetched attributes before compute starts to write to the buffer
		if (needSynchronization())
		{
			std::vector<VkBufferMemoryBarrier> BufferBarriers(EmitterCount);

			// Let compute queue own the buffers
			for (uint32_t i = 0; i < EmitterCount; ++i)
			{
				// Compute shader will write data to the storage buffer
				BufferBarriers[i] = Emitters[i].ComputeOwnBarrier(0, VK_ACCESS_SHADER_WRITE_BIT);
			}
			// Block the compute shader until vertex shader finished reading the storage buffer
			vkCmdPipelineBarrier(CommandBuffer,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,										// No Dependency flag
				0, nullptr,								// Not a Memory barrier
				EmitterCount, BufferBarriers.data(),	// Buffer memory Barriers
				0, nullptr);							// Not a Image memory barrier
		}

		// Dispatch the compute job
		vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ComputePipeline);
		
		for (size_t i = 0; i < Emitters.size(); ++i)
		{
			Emitters[i].Dispatch(CommandBuffer, ComputePipelineLayout);
		}

		// Add barrier to ensure that compute shader has finished writing to the buffer
		// Without this the (rendering) vertex shader may display incomplete results (partial data from last frame)
		if (needSynchronization())
		{
			std::vector<VkBufferMemoryBarrier> BufferBarriers(EmitterCount);

			// Let graphic queue own the buffers
			for (uint32_t i = 0; i < EmitterCount; ++i)
			{
				BufferBarriers[i] = Emitters[i].GraphicOwnBarrier(VK_ACCESS_SHADER_WRITE_BIT, 0);
			}

			// Block the vertex input stage until compute shader has finished
			vkCmdPipelineBarrier(CommandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				0,
				0, nullptr,
				EmitterCount, BufferBarriers.data(),
				0, nullptr);
		}
		Result = vkEndCommandBuffer(CommandBuffer);
		RESULT_CHECK(Result, "Fail to stop recording a compute command buffer");

	}

	void FComputePass::recreateSwapChain()
	{
		cleanupSwapChain();

		createComputePipeline();
		createCommandBuffer();
		recordComputeCommands();
	}

	void FComputePass::cleanupSwapChain()
	{
		vkFreeCommandBuffers(pMainDevice->LD, ComputeCommandPool, 1, &CommandBuffer);
		vkDestroyPipeline(pMainDevice->LD, ComputePipeline, nullptr);
		vkDestroyPipelineLayout(pMainDevice->LD, ComputePipelineLayout, nullptr);
	}

	void FComputePass::prepareDescriptors()
	{
		// 1. Create descriptor pools
		//-------------------------------------------------------
		{
			const uint32_t DescriptorTypeCount = 3;
			const uint32_t MaxDescriptorsPerType = 10;
			VkDescriptorPoolSize PoolSize[DescriptorTypeCount] = {};
			PoolSize[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			PoolSize[0].descriptorCount = MaxDescriptorsPerType;
			PoolSize[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			PoolSize[1].descriptorCount = MaxDescriptorsPerType;
			PoolSize[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			PoolSize[2].descriptorCount = MaxDescriptorsPerType;

			VkDescriptorPoolCreateInfo PoolCreateInfo = {};
			PoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			PoolCreateInfo.maxSets = DescriptorTypeCount * MaxDescriptorsPerType;
			PoolCreateInfo.poolSizeCount = DescriptorTypeCount;
			PoolCreateInfo.pPoolSizes = PoolSize;

			VkResult Result = vkCreateDescriptorPool(pMainDevice->LD, &PoolCreateInfo, nullptr, &DescriptorPool);
			RESULT_CHECK(Result, "Failed to create a compute Descriptor Pool");
		}

		for (size_t i = 0; i < Emitters.size(); ++i)
		{
			Emitters[i].ComputeDescriptorSet.CreateDescriptorSetLayout(ComputePass);
			Emitters[i].ComputeDescriptorSet.AllocateDescriptorSet(DescriptorPool);
			Emitters[i].ComputeDescriptorSet.BindDescriptorWithSet();

			Emitters[i].RenderDescriptorSet.CreateDescriptorSetLayout(ParticlePass_frag);
			Emitters[i].RenderDescriptorSet.AllocateDescriptorSet(DescriptorPool);
			Emitters[i].RenderDescriptorSet.BindDescriptorWithSet();
		}
		

	}

	void FComputePass::createComputePipeline()
	{
		VkDescriptorSetLayout SetLayouts = cDescriptorSet::GetDescriptorSetLayout(EDescriptorSetType::ComputePass);
		// 1. Create pipeline layout according to the descriptor set layout
		VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = {};

		PipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		PipelineLayoutCreateInfo.setLayoutCount = 1;
		PipelineLayoutCreateInfo.pSetLayouts = &SetLayouts;
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