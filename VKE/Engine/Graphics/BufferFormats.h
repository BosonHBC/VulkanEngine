#pragma once
#include "glm/glm.hpp"
namespace VKE
{
	namespace BufferFormats
	{
		// Data update per frame
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

		// Data update per drawcall
		struct FDrawCall
		{
			glm::mat4 ModelMatrix = glm::mat4(1.0f);

			FDrawCall() = default;
			FDrawCall(const glm::mat4& i_model)
			{
				ModelMatrix = i_model;
			}
		};
	}
}