#include "Emitter.h"
#include "Utilities.h"
#include "Descriptors/Descriptor_Buffer.h"

namespace VKE
{
	#define MinRadius 0.00001f
	cEmitter::cEmitter()
	{
		EmitterData.Radius = glm::max(EmitterData.Radius, MinRadius);
	}

	cEmitter::~cEmitter()
	{

	}

	void cEmitter::init(FMainDevice* const iMainDevice)
	{
		ComputeDescriptorSet.pMainDevice = iMainDevice;

		// Create storage buffer
		// Initialize the particle data
		for (size_t i = 0; i < Particle_Count; ++i)
		{
			NextParticle(Particles[i]);
		}
		// Create storage buffer
		VkDeviceSize StorageBufferSize = sizeof(BufferFormats::FParticle) * Particle_Count;
		cBuffer StagingBuffer;

		if (!StagingBuffer.CreateBufferAndAllocateMemory(iMainDevice->PD, iMainDevice->LD, StorageBufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
		{
			return;
		}

		// Map particle data to the staging buffer
		void * pData = nullptr;
		vkMapMemory(iMainDevice->LD, StagingBuffer.GetMemory(), 0, StorageBufferSize, 0, &pData);
		memcpy(pData, Particles, static_cast<size_t>(StorageBufferSize));
		vkUnmapMemory(iMainDevice->LD, StagingBuffer.GetMemory());

		// Create storage buffer, Binding = 0
		ComputeDescriptorSet.CreateStorageBufferDescriptor(StorageBufferSize, 1, VK_SHADER_STAGE_COMPUTE_BIT,
			// 1. As transfer destination from staging buffer, 2. As storage buffer storing particle data in compute shader, 3. As vertex data in vertex shader
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			// Local hosted buffer, need get data from staging buffer 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		// Allocate the transfer command buffer
		VkCommandBuffer TransferCommandBuffer = BeginCommandBuffer(iMainDevice->LD, iMainDevice->GraphicsCommandPool);

		// Region of data to copy from and to, allows copy multiple regions of data
		VkBufferCopy BufferCopyRegion = {};
		BufferCopyRegion.srcOffset = 0;
		BufferCopyRegion.dstOffset = 0;
		BufferCopyRegion.size = StorageBufferSize;

		// Command to copy src buffer to dst buffer
		const cBuffer& StorageBuffer = ComputeDescriptorSet.GetDescriptorAt<cDescriptor_Buffer>(0)->GetBuffer();
		vkCmdCopyBuffer(TransferCommandBuffer, StagingBuffer.GetvkBuffer(), StorageBuffer.GetvkBuffer(), 1, &BufferCopyRegion);
		// Setup a barrier when compute queue is not the same as the graphic queue
		if (iMainDevice->NeedSynchronization())
		{
			VkBufferMemoryBarrier Barrier = {};
			Barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			Barrier.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
			Barrier.dstAccessMask = 0;
			Barrier.srcQueueFamilyIndex = iMainDevice->QueueFamilyIndices.graphicFamily;
			Barrier.dstQueueFamilyIndex = iMainDevice->QueueFamilyIndices.computeFamily;
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
		EndCommandBuffer(TransferCommandBuffer, iMainDevice->LD, iMainDevice->graphicQueue, iMainDevice->GraphicsCommandPool);

		// Clean up staging buffer parts
		StagingBuffer.cleanUp();

		// Create uniform buffer

		// Binding = 1, dt, gravity
		ComputeDescriptorSet.CreateBufferDescriptor(sizeof(BufferFormats::FParticleSupportData), 1, VK_SHADER_STAGE_COMPUTE_BIT);

		// Setup initial data
		ParticleSupportData.dt = 0.0005f;
		ParticleSupportData.useGravity = false;
		ComputeDescriptorSet.GetDescriptorAt<cDescriptor_Buffer>(1)->UpdateBufferData(&ParticleSupportData);

		// Binding = 2, emitter data
		ComputeDescriptorSet.CreateBufferDescriptor(sizeof(BufferFormats::FConeEmitter), 1, VK_SHADER_STAGE_COMPUTE_BIT);
		// Update initial particle data
		UpdateEmitterData(ComputeDescriptorSet.GetDescriptorAt<cDescriptor_Buffer>(2));
	}

	void cEmitter::cleanUp()
	{
		ComputeDescriptorSet.cleanUp();
	}

	void cEmitter::NextParticle(BufferFormats::FParticle& oParticle)
	{
		// Position
		float R = glm::clamp(EmitterData.Radius * sqrt(Rand01()), MinRadius, EmitterData.Radius);
		float theta = Rand01() * 2 * PI;
		
		glm::vec3 oPos = glm::vec3(0.0f);
		float sinTheta = glm::sin(theta);
		float cosTheta = glm::cos(theta);
		oPos.x += R * cosTheta;
		oPos.z += R * sinTheta;
		oParticle.Pos = oPos;

		// Velocity
		float PercentageToCenter = R / EmitterData.Radius;
		float Alpha = PercentageToCenter * EmitterData.Angle;
		float sinAlpha = glm::sin(Alpha);
		float StartSpeed = RandRange(EmitterData.StartSpeedMin, EmitterData.StartSpeedMax);
		oParticle.Vel = glm::vec3(sinAlpha * cosTheta, glm::cos(Alpha), sinAlpha * sinTheta) * StartSpeed;	// normalized * Start Speed

		// Time
		oParticle.LifeTime = RandRange(EmitterData.LifeTimeRangeMin, EmitterData.LifeTimeRangeMax);
		oParticle.ElpasedTime = -RandRange(EmitterData.StartDelayRangeMin, EmitterData.StartDelayRangeMax);
	}

	void cEmitter::UpdateEmitterData(cDescriptor_Buffer* Descriptor)
	{
		if (!Descriptor || !bNeedUpdate)
		{
			return;
		}

		Descriptor->UpdateBufferData(&EmitterData);
		bNeedUpdate = false;
	}

	VkBufferMemoryBarrier cEmitter::GraphicOwnBarrier(VkAccessFlags srcMask, VkAccessFlags dstMask) const
	{
		FMainDevice* const MainDevice = ComputeDescriptorSet.pMainDevice;
		const cBuffer& StorageBuffer = GetStorageBuffer();
		VkBufferMemoryBarrier BufferBarrier = {};
		BufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		BufferBarrier.srcAccessMask = srcMask;
		BufferBarrier.dstAccessMask = dstMask;
		// Transfer ownership from compute queue to graphic queue
		BufferBarrier.srcQueueFamilyIndex = MainDevice->QueueFamilyIndices.computeFamily;
		BufferBarrier.dstQueueFamilyIndex = MainDevice->QueueFamilyIndices.graphicFamily;
		BufferBarrier.buffer = StorageBuffer.GetvkBuffer();
		BufferBarrier.offset = 0;
		BufferBarrier.size = StorageBuffer.BufferSize();

		return BufferBarrier;
	}

	VkBufferMemoryBarrier cEmitter::ComputeOwnBarrier(VkAccessFlags srcMask, VkAccessFlags dstMask) const
	{
		FMainDevice* const MainDevice = ComputeDescriptorSet.pMainDevice;
		const cBuffer& StorageBuffer = GetStorageBuffer();
		VkBufferMemoryBarrier BufferBarrier = {};
		BufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		// The storage buffer is used as vertex input
		BufferBarrier.srcAccessMask = srcMask;
		BufferBarrier.dstAccessMask = dstMask;
		// Transfer ownership from graphic queue to compute queue
		BufferBarrier.srcQueueFamilyIndex = MainDevice->QueueFamilyIndices.graphicFamily;
		BufferBarrier.dstQueueFamilyIndex = MainDevice->QueueFamilyIndices.computeFamily;
		BufferBarrier.buffer = StorageBuffer.GetvkBuffer();
		BufferBarrier.offset = 0;
		BufferBarrier.size = StorageBuffer.BufferSize();

		return BufferBarrier;
	}


	const cBuffer& cEmitter::GetStorageBuffer() const
	{
		return ComputeDescriptorSet.GetDescriptorAt_Immutable<cDescriptor_Buffer>(0)->GetBuffer();
	}

	void cEmitter::Dispatch(const VkCommandBuffer& CommandBuffer, const VkPipelineLayout& ComputePipelineLayout)
	{
		vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ComputePipelineLayout, 0, 1, &ComputeDescriptorSet.GetDescriptorSet(), 0, 0);
		vkCmdDispatch(CommandBuffer, Particle_Count / Dispatch_Size_X, 1, 1);
	}

}