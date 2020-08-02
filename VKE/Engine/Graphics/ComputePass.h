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
		static bool SComputePipelineRequired;
		// LD, PD
		FMainDevice* pMainDevice;

		// Queue
		VkQueue Queue;
		bool needSynchronization() const;

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

		void init(FMainDevice* const iMainDevice);

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

		void createStorageBuffer();
		void createUniformBuffer();
		void prepareDescriptors();
		void createComputePipeline();
		void createCommandPool();
		void createCommandBuffer();
		void createSynchronization();
	};
}