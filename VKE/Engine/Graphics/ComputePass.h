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
#define Particle_Count 16
#define Dispatch_Size_X 16
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
		// *Contain StoratgeBuffer(binding = 0) of particles 
		// *And UniformBuffer(binding = 1) of support data(BufferFormats::FParticleSupportData) for computing particles movement								
		cDescriptorSet ComputeDescriptorSet;									
		// *Actual data of particles and support data
		BufferFormats::FParticle Particles[Particle_Count];
		BufferFormats::FParticleSupportData ParticleSupportData;				// Including deltaTime, will add in the future
		cEmitter Emitter;														// Emitter for this particles

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

		const cBuffer& GetStorageBuffer();

		void undateUniformBuffer();

		void recreateSwapChain();

		void cleanupSwapChain();
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