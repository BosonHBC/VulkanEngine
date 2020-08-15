#pragma once
#include "Transform/Transform.h"
#include "BufferFormats.h"

namespace VKE
{
	/*
	*Base class for all Emitter
	*/
	class cEmitter
	{
	public:
		cEmitter();
		~cEmitter();

		cTransform Transform;
		BufferFormats::FConeEmitter EmitterData;

		void NextParticle(BufferFormats::FParticle& oParticle);
	private:
		
	};

}