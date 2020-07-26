#pragma once

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "Transform/Transform.h"
#include "BufferFormats.h"

namespace VKE
{
	class cCamera
	{
	public:
		/** Constructors and destructor */
		cCamera() : m_translationSpeed(1), m_turnSpeed(0.1f)
		{

		}
		cCamera(glm::vec3 i_initialPos, float i_initialPitch = 0.0, float i_initialYaw = 0.0, float i_moveSpeed = 1.0, float i_turnSpeed = 1.0f) :
			m_yaw(i_initialYaw), m_pitch(i_initialPitch), m_worldUp(glm::vec3(0, 1.f, 0)),
			m_translationSpeed(i_moveSpeed), m_turnSpeed(i_turnSpeed)
		{
			glm::quat _pitch(glm::vec3(i_initialPitch, 0, 0));
			glm::quat _yaw(glm::vec3(0, i_initialYaw, 0));
			Transform.SetTransform(i_initialPos, _pitch * _yaw, glm::vec3(1, 1, 1));
		}
		cCamera(const cCamera& i_other) = delete;
		cCamera(const cCamera&& i_other) = delete;
		cCamera& operator = (const cCamera& i_other) = delete;
		cCamera& operator = (const cCamera&& i_other) = delete;
		virtual ~cCamera() {};

		/** Usage functions*/
		void Update();
		void MoveRight(float iAxis);
		void MoveForward(float iAxis);
		void MoveUp(float iAxis);
		void TurnCamera();
		void ZoomCamera();

		// Projection matrix
		void UpdateProjectionMatrix(float i_fov, float i_aspect, float i_nearPlane = 0.1f, float i_farPlane = 100.f);

		void MirrorAlongPlane(const cTransform& i_plane);

		/** Getters*/
		const glm::mat4& GetViewMatrix() { return FrameData.ViewMatrix; }
		glm::mat4 GetInvViewMatrix();
		const glm::mat4& GetProjectionMatrix() const { return FrameData.ProjectionMatrix; }
		glm::mat4 GetInvprojectionMatrix();

		// Get Camera location
		glm::vec3 CamLocation() const { return Transform.Position(); }
		
		// Get Frame data
		BufferFormats::FFrame& GetFrameData() { return FrameData; }
		
		// Transform of the camera
		cTransform Transform;
	protected:
		/** private member variables*/
		// Only when there is input changing the camera update the camera
		bool bNeedUpdateCamera = false;

		float m_translationSpeed = 1.0f;
		float m_turnSpeed = 1.0f;
		float m_pitch = 0.0f;
		float m_yaw = 0.0f;

		BufferFormats::FFrame FrameData;

		glm::vec3 m_worldUp = glm::vec3(0, 1.f, 0);

	};

}