#pragma once
#include "glm/glm.hpp"

// Plug-ins
#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
// Marcos
#define IsFloatZero(x) (x > -0.0001f && x < 0.0001f)

struct GLFWwindow;
namespace VKE
{
	class cCamera;

	int init();

	void run();

	void cleanup();

	GLFWwindow* GetGLFWWindow();
	cCamera* GetCurrentCamera();
	glm::ivec2 GetWindowExtent();
	double dt();

	glm::vec2 GetMouseDelta();
	extern uint64_t ElapsedFrame;
	extern bool bWindowIconified;

}