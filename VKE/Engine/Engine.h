#pragma once
struct GLFWwindow;
namespace VKE
{
	int init();

	void run();

	void cleanup();

	GLFWwindow* GetGLFWWindow();
}