#include "Camera.h"
#include "Engine.h"

#include "glfw/glfw3.h"

#include "stdio.h"
namespace VKE
{
	void cCamera::Update()
	{
		if (bNeedUpdateCamera)
		{
			bNeedUpdateCamera = false;
			// Clamp the domain of pitch and yaw
			m_pitch = glm::clamp(m_pitch, -89.f, 89.f);
			glm::vec3 _forward = m_worldUp * sin(glm::radians(m_pitch)) + cos(glm::radians(m_pitch)) * (cTransform::WorldForward * cos(glm::radians(m_yaw)) + cTransform::WorldRight * sin(glm::radians(m_yaw)));
			glm::vec3 _right = glm::normalize(glm::cross(_forward, m_worldUp));
			glm::vec3 _up = glm::normalize(glm::cross(_right, _forward));

			Transform.SetRotation(glm::quatLookAt(_forward, _up));
			Transform.Update();

			glm::vec3 _targetLoc = Transform.Position() + Transform.Forward();

			FrameData.ViewMatrix = glm::lookAt(Transform.Position(), _targetLoc, m_worldUp);

			FrameData.Update();
		}
	}

	void cCamera::MoveRight(float iAxis)
	{
		if (!IsFloatZero(iAxis))
		{
			Transform.Translate(-Transform.Right() * m_translationSpeed  * iAxis * (float)dt());
			bNeedUpdateCamera = true;
		}
	}

	void cCamera::MoveForward(float iAxis)
	{
		if (!IsFloatZero(iAxis))
		{
			Transform.Translate(Transform.Forward() * m_translationSpeed * iAxis* (float)dt());
			bNeedUpdateCamera = true;
		}
	}


	void cCamera::MoveUp(float iAxis)
	{
		if (!IsFloatZero(iAxis))
		{
			Transform.Translate(m_worldUp * m_translationSpeed * iAxis* (float)dt());
			bNeedUpdateCamera = true;
		}
	}

	void cCamera::TurnCamera()
	{
		glm::vec2 dMouse = GetMouseDelta();
		if (!IsFloatZero(dMouse.x) || !IsFloatZero(dMouse.y))
		{
			m_yaw += -dMouse.x * m_turnSpeed;
			m_pitch += dMouse.y * m_turnSpeed;
			bNeedUpdateCamera = true;
		}
	}

	void cCamera::ZoomCamera()
	{
		//printf("Zooming\n");
	}

	glm::mat4 cCamera::GetInvViewMatrix()
	{
		return FrameData.InvView;
	}

	glm::mat4 cCamera::GetInvprojectionMatrix()
	{
		return FrameData.InvProj;
	}

	void cCamera::UpdateProjectionMatrix(float i_fov, float i_aspect, float i_nearPlane /*= 0.1f*/, float i_farPlane /*= 100.f*/)
	{
		FrameData.ProjectionMatrix = glm::perspective(i_fov, i_aspect, i_nearPlane, i_farPlane);
		FrameData.ProjectionMatrix[1][1] *= -1;				// inverting Y axis since glm treats
		
		bNeedUpdateCamera = true;
	}

	void cCamera::MirrorAlongPlane(const cTransform & i_plane)
	{
		// mirror the transform;
		float deltaY = (Transform.Position() - i_plane.Position()).y;
		Transform.Translate(glm::vec3(0, -2.f*deltaY, 0));
		m_worldUp = -m_worldUp;

		bNeedUpdateCamera = true;
	}
}