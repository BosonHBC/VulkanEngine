#pragma once
#include "Transform/Transform.h"
namespace VKE
{
	class cLight
	{
	public:
		// render debug shape of the light
		virtual void DebugRenderLight() { /* Override in subclass*/ };
		
		virtual void Illuminate() = 0;

		cTransform Transform;
		uint8_t bDebugRendering = false;
	private:
		

	};
}