#pragma once
#include "Core.h"
#include "glm/glm.hpp"

// Plug-ins
#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

struct GLFWwindow;
namespace VKE
{
	class cCamera;

	int32 Init();

	void Run();

	void Cleanup();

	GLFWwindow* GetGLFWWindow();
	cCamera* GetCurrentCamera();
	glm::ivec2 GetWindowExtent();

	glm::vec2 GetMouseDelta();
	extern uint64_t ElapsedFrame;
	extern bool bWindowIconified;
}