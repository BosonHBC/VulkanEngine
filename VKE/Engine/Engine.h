#pragma once
// Plug-ins
#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE


struct GLFWwindow;
namespace VKE
{
	int init();

	void run();

	void cleanup();

	GLFWwindow* GetGLFWWindow();
}