#pragma once
#include "Transform/Transform.h"
#include "BufferFormats.h"
#include "Texture/Texture.h"
#include "Utilities.h"
#include "Descriptors/DescriptorSet.h"
#include "Buffer/Buffer.h"
namespace VKE
{
	class cDescriptor_Buffer;
	/*
	*Base class for all Emitter
	*/
	class cEmitter
	{
	public:
		cEmitter();
		~cEmitter();

		void init(FMainDevice* const iMainDevice);
		void cleanUp();

		BufferFormats::FParticle Particles[Particle_Count];
		BufferFormats::FParticleSupportData ParticleSupportData;				// Including deltaTime, will add in the future
		std::shared_ptr<cTexture> TextureToUse;									// Texture to use in this emitter
		cTransform Transform;
		BufferFormats::FConeEmitter EmitterData;
		bool bNeedUpdate = true;

		cDescriptorSet ComputeDescriptorSet;		// Used in compute shader Storage buffer, uniform buffer
		cDescriptorSet RenderDescriptorSet;			// Used in vertex / fragment shader when rendering, currently is for sampler

		void NextParticle(BufferFormats::FParticle& oParticle);
		void UpdateEmitterData(cDescriptor_Buffer* Descriptor);
		/** Graphic queue acquires ownership of this storage buffer */
		VkBufferMemoryBarrier GraphicOwnBarrier( VkAccessFlags srcMask, VkAccessFlags dstMask) const;
		/** Compute queue acquires ownership of this storage buffer */
		VkBufferMemoryBarrier ComputeOwnBarrier(VkAccessFlags srcMask, VkAccessFlags dstMask) const;

		const cBuffer& GetStorageBuffer() const;

		void Dispatch(const VkCommandBuffer& CommandBuffer, const VkPipelineLayout& ComputePipelineLayout);
	private:
		
	};

}