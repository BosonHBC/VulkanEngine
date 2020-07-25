// Engines
#include "Engine.h"

#include "Graphics/VKRenderer.h"


#include "glm/glm.hpp"
#include "glm/mat4x4.hpp"
// Systems
#include "stdio.h"
#include <string>
#include "Model/Model.h"

namespace VKE {
	//=================== Parameters =================== 
	GLFWwindow* g_Window;
	VKRenderer* g_renderer;
	const GLuint WIDTH = 800, HEIGHT = 600;
	const std::string WINDOW_NAME = "Default";

	//=================== Function declarations =================== 
	
	// GLFW
	void initGLFW();
	void cleanupGLFW();
	

	int init()
	{
		int result = EXIT_SUCCESS;
		initGLFW();

		g_renderer = DBG_NEW VKRenderer();
		if (g_renderer->init(g_Window) == EXIT_FAILURE)
		{
			return EXIT_FAILURE;
		}
		
		// Create Mesh
		cModel* pContainerModel = nullptr;

		g_renderer->CreateModel("Container.obj", pContainerModel);
		g_renderer->RenderList.push_back(pContainerModel);

		pContainerModel->Transform.SetTransform(glm::vec3(0, -2, -5), glm::quat(1, 0, 0, 0), glm::vec3(0.01f, 0.01f, 0.01f));
		pContainerModel->Transform.gRotate(glm::vec3(0, 1, 0), 45);
		pContainerModel->Transform.Update();

		return result;
	}

	void run()
	{
		if (!g_Window)
		{
			return;
		}

		double DeltaTime = 0.0f;
		double LastTime = glfwGetTime();

		while (!glfwWindowShouldClose(g_Window))
		{
			glfwPollEvents();
			double now = glfwGetTime();
			DeltaTime = now - LastTime;
			LastTime = now;

			g_renderer->tick((float)DeltaTime);
			g_renderer->draw();
		}
	}

	void cleanup()
	{
		g_renderer->cleanUp();
		safe_delete(g_renderer);

		cleanupGLFW();
	}


	GLFWwindow* GetGLFWWindow()
	{
		return g_Window;
	}

	void initGLFW()
	{
		glfwInit();

		// using Vulkan here
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

		g_Window = glfwCreateWindow(WIDTH, HEIGHT, WINDOW_NAME.c_str(), nullptr, nullptr);

	}
	void cleanupGLFW()
	{
		glfwDestroyWindow(g_Window);

		glfwTerminate();
	}




}