

#include "Engine.h"
#define safe_delete(x) if(x!=nullptr) {delete x; x = nullptr; }

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/mat4x4.hpp"

#include "stdio.h"

#include "assert.h"

namespace VKE {
	GLFWwindow* g_Window;

	void init()
	{
		glfwInit();

		// using Vulkan here
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		g_Window = glfwCreateWindow(800, 600, "Test Window", nullptr, nullptr);

		uint32_t ExtensionCount = 0;
		// vulkan extension count
		vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, nullptr);

		printf("Extension Count: %i", ExtensionCount);

	}

	void run()
	{
		if (!g_Window)
		{
			return;
		}

		while (!glfwWindowShouldClose(g_Window))
		{
			glfwPollEvents();
		}
	}

	void cleanup()
	{
		
		glfwDestroyWindow(g_Window);

		glfwTerminate();
	}

}