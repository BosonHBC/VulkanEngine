#pragma once
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "Engine.h"
#include "Utilities.h"
#include "Buffer/Buffer.h"
#include "BufferFormats.h"
#include "Descriptors/DescriptorSet.h"
#include "ParticleSystem/Emitter.h"

namespace VKE
{

	struct FComputePass
	{
		FComputePass() {}

		static bool SComputePipelineRequired;
		// LD, PD
		FMainDevice* pMainDevice;

		// Queue
		VkQueue ComputeQueue;
		bool needSynchronization() const;

		// Command related
		VkCommandPool ComputeCommandPool;
		VkCommandBuffer CommandBuffer;

		// Descriptor related
		VkDescriptorPool DescriptorPool;			
	
		std::vector<cEmitter> Emitters;											// Emitter for this particles

		// Pipeline related
		VkPipelineLayout ComputePipelineLayout;
		VkPipeline ComputePipeline;

		// Synchronization related
		// Signal when compute pass is finished
		VkSemaphore OnComputeFinished;

		void init(FMainDevice* const iMainDevice);

		void cleanUp();

		// Compute pass commands, no need to re-record like graphic commands
		void recordComputeCommands();

		void undateUniformBuffer();

		void recreateSwapChain();

		void cleanupSwapChain();

		bool bNeedComputePass = true;
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