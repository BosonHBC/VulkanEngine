// Engines
#include "Engine.h"

#include "Graphics/VKRenderer.h"
#include "Input/UserInput.h"

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
	UserInput::FUserInput* g_Input;
	const GLuint WIDTH = 800, HEIGHT = 600;
	const std::string WINDOW_NAME = "Default";

	//=================== Function declarations =================== 
	
	// GLFW
	void initGLFW();
	void cleanupGLFW();
	// Input
	void initInput();
	void cleanupInput();

	int init()
	{
		int result = EXIT_SUCCESS;
		initGLFW();
		initInput();

		g_renderer = DBG_NEW VKRenderer();
		if (g_renderer->init(g_Window) == EXIT_FAILURE)
		{
			return EXIT_FAILURE;
		}
		
		// Create Mesh
		cModel* pContainerModel = nullptr;
		cModel* pPlaneModel = nullptr;

		g_renderer->CreateModel("Container.obj", pContainerModel);
		g_renderer->RenderList.push_back(pContainerModel);

		g_renderer->CreateModel("Plane.obj", pPlaneModel);
		g_renderer->RenderList.push_back(pPlaneModel);

		pContainerModel->Transform.SetTransform(glm::vec3(0, -2, -5), glm::quat(1, 0, 0, 0), glm::vec3(0.01f, 0.01f, 0.01f));

		pPlaneModel->Transform.SetTransform(glm::vec3(0, 0, 0), glm::quat(1, 0, 0, 0), glm::vec3(25,25,25));
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
			g_Input->UpdateInput();

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

		cleanupInput();
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

	void testDeleSpace()
	{
		printf("Space pressed\n");
	}
	void testAxis(float value)
	{
		printf("Axis value: %.2f\n", value);
	}
	void initInput()
	{
		using namespace UserInput;
		g_Input = DBG_NEW UserInput::FUserInput();
		g_Input->AddActionKeyPairToMap("Jump", EKeyCode::Space);
		g_Input->BindAction("Jump", IT_OnPressed, &testDeleSpace);

		g_Input->AddAxisKeyPairToMap("Horizontal", { EKeyCode::A,EKeyCode::D });
		g_Input->BindAxis("Horizontal", &testAxis);
	}

	void cleanupInput()
	{
		delete g_Input;
	}

}