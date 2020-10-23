#pragma once
#include "Light.h"
#include "Misc/Shape/Sphere.h"
namespace VKE
{

	struct FPointLight
	{
		glm::vec3 LightColor = glm::vec3(1.0f);
		float Radius = 1.0f;
		glm::vec3 Position = glm::vec3(0.0f);
		float Intensity;
	};
	class cPointLight : public cLight
	{
	public:

		cPointLight() = default;
		cPointLight(const FPointLight& InLightData) : LightData(InLightData) 
		{
			Transform.SetPosition(InLightData.Position);
			Transform.Update();
		}

		// Start of cLight Interface
		void DebugRenderLight() override {}
		void Illuminate() override {}
		// End of cLight Interface
	public:
		FPointLight LightData;
	};
}