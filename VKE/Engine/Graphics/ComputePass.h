#pragma once
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "Engine.h"
#include "Utilities.h"
#include "Buffer/Buffer.h"
#include "Descriptors/Descriptor.h"
#include "BufferFormats.h"

namespace VKE
{
#define Particle_Count 256
	struct FComputePass
	{
		// LD, PD
		FMainDevice* pMainDevice;

		// Queue
		VkQueue Queue;
		int ComputeFamilyIndex = -1;
		bool needSynchronization() const;
		bool isQueueValid() const { return ComputeFamilyIndex >= 0; }

		// Buffers
		cBuffer StorageBuffer;		// Buffer for particle data
		BufferFormats::FParticle Particles[Particle_Count];

		cDescriptor UniformBuffer;										// Buffer for support data(BufferFormats::FParticleSupportData) for computing particles movement
		BufferFormats::FParticleSupportData ParticleSupportData;		// Including deltaTime, will add in the future

		// Command related
		VkCommandPool CommandPool;
		VkCommandBuffer CommandBuffer;

		// Descriptor related
		VkDescriptorPool DescriptorPool;
		VkDescriptorSetLayout DescriptorSetLayout;
		VkDescriptorSet DescriptorSet;

		// Pipeline related
		VkPipelineLayout ComputePipelineLayout;
		VkPipeline ComputePipeline;

		// Synchronization related
		// Signal when compute pass is finished
		VkSemaphore OnComputeFinished;

		bool init(FMainDevice* const iMainDevice)
		{
			pMainDevice = iMainDevice;
			// 1. Get compute queue and queue family
			vkGetDeviceQueue(pMainDevice->LD, ComputeFamilyIndex, 0, &Queue);

			// . create storage buffer and uniform buffer
			createStorageBuffer();
			createUniformBuffer();
			// . set up descriptor set related
			prepareDescriptors();
			// . Create compute pipeline
			createComputePipeline();
			// . Create compute command pool
			createCommandPool();
			// . Create command buffer
			createCommandBuffer();
			// . Create semaphores and fences
			createSynchronization();
		}

		void cleanUp()
		{
			// wait until the device is not doing anything (nothing on any queue)
			vkDeviceWaitIdle(pMainDevice->LD);

			vkDestroySemaphore(pMainDevice->LD, OnComputeFinished, nullptr);

			vkDestroyCommandPool(pMainDevice->LD, CommandPool, nullptr);
			vkDestroyPipeline(pMainDevice->LD, ComputePipeline, nullptr);
			vkDestroyPipelineLayout(pMainDevice->LD, ComputePipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(pMainDevice->LD, DescriptorSetLayout, nullptr);
			vkDestroyDescriptorPool(pMainDevice->LD, DescriptorPool, nullptr);
			UniformBuffer.cleanUp();
			StorageBuffer.cleanUp();
		}

		void recordCommands()
		{

		}

	private:

		bool createStorageBuffer()
		{
			const glm::vec3 initialLocMin(-0.10, 1.50, -0.10), initialLocMax(0.10, 1.60, 0.10), initialVelMin(-1.00, 0, -1.00), initialVelMax(1.00, 1.00, 1.00);
			// Initialize the particle data
			for (size_t i = 0; i < Particle_Count; ++i)
			{
				Particles[i].Pos = glm::vec4(RandRange(initialLocMin, initialVelMax), 1.0f);
				Particles[i].Vel = glm::vec4(RandRange(initialVelMin, initialVelMax), 1.0f);
			}
			// Create storage buffer
			VkDeviceSize StorageBufferSize = sizeof(BufferFormats::FParticle) * Particle_Count;
			cBuffer StagingBuffer;

			if (!StagingBuffer.CreateBufferAndAllocateMemory(pMainDevice->PD, pMainDevice->LD, StorageBufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			{
				return false;
			}

			// Map particle data to the staging buffer
			void * pData = nullptr;
			vkMapMemory(pMainDevice->LD, StagingBuffer.GetMemory(), 0, StorageBufferSize, 0, &pData);
			memcpy(pData, Particles, static_cast<size_t>(StorageBufferSize));
			vkUnmapMemory(pMainDevice->LD, StagingBuffer.GetMemory());

			// Create storage buffer
			if (!StorageBuffer.CreateBufferAndAllocateMemory(pMainDevice->PD, pMainDevice->LD, StorageBufferSize,
				// 1. As transfer destination from staging buffer, 2. As storage buffer storing particle data in compute shader, 3. As vertex data in vertex shader
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			{
				return false;
			}

			// Allocate the transfer command buffer
			VkCommandBuffer TransferCommandBuffer = BeginCommandBuffer(pMainDevice->LD, CommandPool);

			// Region of data to copy from and to, allows copy multiple regions of data
			VkBufferCopy BufferCopyRegion = {};
			BufferCopyRegion.srcOffset = 0;
			BufferCopyRegion.dstOffset = 0;
			BufferCopyRegion.size = StorageBufferSize;

			// Command to copy src buffer to dst buffer
			vkCmdCopyBuffer(TransferCommandBuffer, StagingBuffer.GetBuffer(), StorageBuffer.GetBuffer(), 1, &BufferCopyRegion);
			// Setup a barrier when compute queue is not the same as the graphic queue
			if (needSynchronization())
			{
				VkBufferMemoryBarrier Barrier = {};
				Barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
				Barrier.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
				Barrier.dstAccessMask = 0;
				Barrier.srcQueueFamilyIndex = pMainDevice->QueueFamilyIndices.graphicFamily;
				Barrier.dstQueueFamilyIndex = ComputeFamilyIndex;
				Barrier.buffer = StorageBuffer.GetBuffer();
				Barrier.offset = 0;
				Barrier.size = StorageBufferSize;

				vkCmdPipelineBarrier( TransferCommandBuffer,
					VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 
					0, nullptr,			// memory
					1, &Barrier,		// Buffer
					0, nullptr);		// Image
			}
			// Stop the command and submit it to the queue, wait until it finish execution
			EndCommandBuffer(TransferCommandBuffer, pMainDevice->LD, Queue, CommandPool);

			// Clean up staging buffer parts
			StagingBuffer.cleanUp();
		}
		bool createUniformBuffer()
		{
			// Only 1 supportData is needed
			UniformBuffer.SetDescriptorBufferRange(sizeof(BufferFormats::FParticleSupportData), 1);
			UniformBuffer.CreateDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, pMainDevice->PD, pMainDevice->LD);
			// Setup initial data
			BufferFormats::FParticleSupportData Temp = {};
			Temp.dt = 0.01f;
			UniformBuffer.UpdateBufferData(&Temp);
		}
		void prepareDescriptors()
		{
			// 1. Create descriptor pools
			//-------------------------------------------------------
			const uint32_t DescriptorTypeCount = 2;
			VkDescriptorPoolSize PoolSize[DescriptorTypeCount] = {};
			PoolSize[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			PoolSize[0].descriptorCount = 1;
			PoolSize[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			PoolSize[1].descriptorCount = 1;

			VkDescriptorPoolCreateInfo PoolCreateInfo = {};
			PoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			PoolCreateInfo.maxSets = 2;
			PoolCreateInfo.poolSizeCount = DescriptorTypeCount;
			PoolCreateInfo.pPoolSizes = PoolSize;

			VkResult Result = vkCreateDescriptorPool(pMainDevice->LD, &PoolCreateInfo, nullptr, &DescriptorPool);
			RESULT_CHECK(Result, "Failed to create a compute Descriptor Pool");


			// 2. Create Descriptor layout
			//-------------------------------------------------------
			const uint32_t BindingCount = 2;
			VkDescriptorSetLayoutBinding Bindings[BindingCount] = {};
			// Storage buffer binding info
			Bindings[0].binding = 0;
			Bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			Bindings[0].descriptorCount = 1;
			Bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;				// only be used in compute shader in compute pipeline
			Bindings[0].pImmutableSamplers = nullptr;

			// particle support data binding info
			Bindings[1] = UniformBuffer.ConstructDescriptorSetLayoutBinding();

			VkDescriptorSetLayoutCreateInfo LayoutCreateInfo;
			LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			LayoutCreateInfo.bindingCount = BindingCount;
			LayoutCreateInfo.pBindings = Bindings;
			LayoutCreateInfo.pNext = nullptr;
			LayoutCreateInfo.flags = 0;

			VkResult Result = vkCreateDescriptorSetLayout(pMainDevice->LD, &LayoutCreateInfo, nullptr, &DescriptorSetLayout);
			RESULT_CHECK(Result, "Fail to create compute descriptor set layout.")
		
			// 3. Allocate Descriptor sets
			//-------------------------------------------------------
			VkDescriptorSetAllocateInfo SetAllocInfo = {};
			SetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			SetAllocInfo.descriptorPool = DescriptorPool;												
			SetAllocInfo.descriptorSetCount = 1;				
			SetAllocInfo.pSetLayouts = &DescriptorSetLayout;

			// Allocate descriptor sets (multiple)
			VkResult Result = vkAllocateDescriptorSets(pMainDevice->LD, &SetAllocInfo, &DescriptorSet);
			RESULT_CHECK(Result, "Fail to Allocate Compute Descriptor Set!");
			
			UniformBuffer.SetDescriptorSet(&DescriptorSet);

			// 4. Update descriptor set write
			//-------------------------------------------------------
			const uint32_t DescriptorCount = 2;
			// Data about connection between Descriptor and buffer
			VkWriteDescriptorSet SetWrites[DescriptorCount] = {};
			// Storage buffer DESCRIPTOR
			SetWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			SetWrites[0].dstSet = DescriptorSet;
			SetWrites[0].dstBinding = 0;
			SetWrites[0].dstArrayElement = 0;
			SetWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			SetWrites[0].descriptorCount = 1;
			VkDescriptorBufferInfo StorageBufferInfo = {};
			StorageBufferInfo.buffer = StorageBuffer.GetBuffer();
			StorageBufferInfo.offset = 0;
			StorageBufferInfo.range = StorageBuffer.BufferSize();
			SetWrites[0].pBufferInfo = &StorageBufferInfo;
			
			// particle support data DESCRIPTOR
			SetWrites[1] = UniformBuffer.ConstructDescriptorBindingInfo();

			// Update the descriptor sets with new buffer / binding info
			vkUpdateDescriptorSets(pMainDevice->LD, DescriptorCount, SetWrites,
				0, nullptr // Allows copy descriptor set to another descriptor set
			);
		}
		void createComputePipeline()
		{
			// 1. Create pipeline layout according to the descriptor set layout
			VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = {};

			PipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			PipelineLayoutCreateInfo.setLayoutCount = 1;
			PipelineLayoutCreateInfo.pSetLayouts = &DescriptorSetLayout;
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
		void createCommandPool()
		{
			VkCommandPoolCreateInfo cmdPoolInfo = {};
			cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			cmdPoolInfo.queueFamilyIndex = ComputeFamilyIndex;
			cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			auto Result = vkCreateCommandPool(pMainDevice->LD, &cmdPoolInfo, nullptr, &CommandPool);
			RESULT_CHECK(Result, "Fail to create Compute Command Pool\n");
		}
		void createCommandBuffer()
		{
			VkCommandBufferAllocateInfo cbAllocInfo = {};						
			cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			cbAllocInfo.commandPool = CommandPool;						
			cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;				
																				
			cbAllocInfo.commandBufferCount = 1;

			VkResult Result = vkAllocateCommandBuffers(pMainDevice->LD, &cbAllocInfo, &CommandBuffer);
			RESULT_CHECK(Result, "Fail to allocate compute command buffers.");
		}
		void createSynchronization()
		{
			VkSemaphoreCreateInfo SemaphoreCreateInfo = {};
			SemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			VkResult Result = vkCreateSemaphore(pMainDevice->LD, &SemaphoreCreateInfo,nullptr, &OnComputeFinished);
			RESULT_CHECK(Result, "Fail to create OnComputeFinish Semaphore");

			// Need to signal in init stage because the graphic pass is running before the compute pass, so making sure no need to wait at for the compute pass in the first render
			// Signal the semaphore with an empty queue (no wait, no command buffer, no fence to signal)
			VkSubmitInfo SubmitInfo = {};
			SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			SubmitInfo.signalSemaphoreCount = 1;
			SubmitInfo.pSignalSemaphores = &OnComputeFinished;

			VkResult Result = vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE);
			RESULT_CHECK(Result, "Fail to submit initial compute queue");
			// Wait for the queue to finish
			Result = vkQueueWaitIdle(Queue);
			RESULT_CHECK(Result, "Fail to wait the queue to finish");
		}
	};
}