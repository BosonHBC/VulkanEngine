#pragma once
#include "glm/glm.hpp"
namespace VKE
{
	namespace BufferFormats
	{
		/** Data update per frame */
		struct FFrame
		{
			glm::mat4 PVMatrix = glm::mat4(1.0f);
			glm::mat4 ProjectionMatrix = glm::mat4(1.0f);
			glm::mat4 InvProj = glm::mat4(1.0f);
			glm::mat4 ViewMatrix = glm::mat4(1.0f);
			glm::mat4 InvView = glm::mat4(1.0f);

			FFrame() = default;
			FFrame(const glm::mat4& i_projectionMatrix, const glm::mat4& i_viewMatrix)
			{
				PVMatrix = i_projectionMatrix * i_viewMatrix;
				ProjectionMatrix = i_projectionMatrix;
				ViewMatrix = i_viewMatrix;
				InvView = glm::inverse(i_viewMatrix);
				InvProj = glm::inverse(i_projectionMatrix);
			}

			void Update()
			{
				PVMatrix = ProjectionMatrix * ViewMatrix;
				InvView = glm::inverse(ViewMatrix);
				InvProj = glm::inverse(ProjectionMatrix);
			}

			glm::vec3 GetViewPosition() const {
				return glm::vec3(InvView[3][0], InvView[3][1], InvView[3][2]);
			}
		};

		/** Data update per drawcall */
		struct FDrawCall
		{
			glm::mat4 ModelMatrix = glm::mat4(1.0f);

			FDrawCall() = default;
			FDrawCall(const glm::mat4& i_model)
			{
				ModelMatrix = i_model;
			}
		};

		/** Particle Data */
		struct FParticle
		{
			glm::vec3 Pos;
			float ElpasedTime;							
			
			glm::vec3 Vel;
			float LifeTime;								// Life time of this particle
			
			glm::vec4 ColorOverlay = glm::vec4(1.0);	// Particle Color multiplier
			
			float Volume = 1.0;							// Particle Size multiplier
			float RotationAlongZ = 0.0;					// Billboard rotation
			int Padding[2];
		};

		/** Emitter Data */
		struct FConeEmitter
		{
			float Radius = 1.0f;
			float Angle = 45.f;		// in degree
			float StartSpeedMin = 1.0f;
			float StartSpeedMax = 1.0f;

			float StartDelayRangeMin = 0.0f;
			float StartDelayRangeMax = 2.0f;
			float LifeTimeRangeMin = 2.0f;
			float LifeTimeRangeMax = 3.0f;

			glm::vec4 StartColor = glm::vec4(1.0);
			glm::vec4 ColorOverLifeTimeStart = glm::vec4(1.0);
			glm::vec4 ColorOverLifeTimeEnd = glm::vec4(1.0);

			float StartSizeMin = 1.0;
			float StartSizeMax = 1.0;
			float NoiseMin = 0.0f;
			float NoiseMax = 0.0f;

			float StartRotationMin = 0.0f;
			float StartRotationMax = 0.0f;
		};

		/** Support data for particles */
		struct FParticleSupportData
		{
			float dt;
			unsigned int useGravity;
		};
	}
}