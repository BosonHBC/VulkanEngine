// Engines
#include "Engine.h"

#include "Graphics/VKRenderer.h"
#include "Input/UserInput.h"
#include "Camera.h"
#include "Editor/Editor.h"
#include "Time.h"
// glm
#include "glm/glm.hpp"
#include "glm/mat4x4.hpp"
// Systems
#include "stdio.h"
#include <string>
#include "Model/Model.h"

namespace VKE {
	const GLuint WIDTH = 1280, HEIGHT = 720;
	const std::string WINDOW_NAME = "Default";
	uint64_t ElapsedFrame = 0;
	bool bWindowIconified = false;

	//=================== Parameters =================== 
	GLFWwindow* g_Window;
	VKRenderer* g_Renderer;
	UserInput::FUserInput* g_Input;
	cCamera* g_Camera;

	//=================== Function declarations =================== 

	// GLFW
	void initGLFW();
	void cleanupGLFW();
	// Input
	void initInput();
	void window_focus_callback(GLFWwindow* window, int focused);
	void window_iconify_callback(GLFWwindow* window, int iconified);
	void cleanupInput();
	void quitApp();

	// Camera
	void initCamera();
	void cleanupCamera();

	int init()
	{
		int result = EXIT_SUCCESS;
		initGLFW();
		initInput();
		initCamera();
		
		g_Renderer = DBG_NEW VKRenderer();
		if (g_Renderer->init(g_Window) == EXIT_FAILURE)
		{
			return EXIT_FAILURE;
		}
		Editor::Init(g_Window, g_Renderer);

		return result;
	}

	void run()
	{
		if (!g_Window)
		{
			return;
		}

		double LastTime = glfwGetTime();
		while (!glfwWindowShouldClose(g_Window))
		{
			glfwPollEvents();

			double now = glfwGetTime();
			Time::DT = now - LastTime;
			
			LastTime = now;
			
			// update editor
			Editor::Update(g_Renderer);
			
			// not minimized
			if (!bWindowIconified && !Editor::GbMovingWindow)
			{
				g_Input->Update();

				g_Camera->Update();

				g_Renderer->tick((float)Time::DT);
				g_Renderer->draw();
			}
		}
	}

	void cleanup()
	{
		Editor::CleanUp(g_Renderer);

		g_Renderer->cleanUp();
		safe_delete(g_Renderer);

		cleanupCamera();
		cleanupInput();
		cleanupGLFW();
	}


	GLFWwindow* GetGLFWWindow()
	{
		return g_Window;
	}

	cCamera* GetCurrentCamera()
	{
		return g_Camera;
	}

	glm::ivec2 GetWindowExtent()
	{
		return glm::ivec2(WIDTH, HEIGHT);
	}

	glm::vec2 GetMouseDelta()
	{
		if (g_Input)
		{
			return g_Input->GetMouseDelta();
		}
		return glm::vec2(0.0f);
	}

	void initGLFW()
	{
		glfwInit();

		// using Vulkan here
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

		g_Window = glfwCreateWindow(WIDTH, HEIGHT, WINDOW_NAME.c_str(), nullptr, nullptr);

		glfwSetWindowFocusCallback(g_Window, window_focus_callback);
		glfwSetWindowIconifyCallback(g_Window, window_iconify_callback);
	}
	void cleanupGLFW()
	{
		glfwDestroyWindow(g_Window);

		glfwTerminate();
	}

	void initInput()
	{
		using namespace UserInput;
		g_Input = DBG_NEW UserInput::FUserInput();
		g_Input->AddActionKeyPairToMap("Escape", EKeyCode::Escape);
		g_Input->BindAction("Escape", EIT_OnPressed, &quitApp);

		g_Input->AddActionKeyPairToMap("TurnCamera", EKeyCode::LMB);
		g_Input->AddActionKeyPairToMap("ZoomCamera", EKeyCode::RMB);

		g_Input->AddAxisKeyPairToMap("MoveRight", { EKeyCode::A,EKeyCode::D });
		g_Input->AddAxisKeyPairToMap("MoveForward", { EKeyCode::S,EKeyCode::W });
		g_Input->AddAxisKeyPairToMap("MoveUp", { EKeyCode::Control,EKeyCode::Space });

	}
	void window_focus_callback(GLFWwindow* window, int focused)
	{
		if (g_Input)
		{
			g_Input->bAppInFocus = focused;
		}
	}
	void window_iconify_callback(GLFWwindow* window, int iconified)
	{
		bWindowIconified = iconified > 0;
	}
	void quitApp()
	{
		glfwSetWindowShouldClose(g_Window, true);
	}
	void cleanupInput()
	{
		safe_delete(g_Input);
	}

	void initCamera()
	{
		g_Camera = DBG_NEW cCamera(glm::vec3(0, 1, 2.5), 15.f, 0.0f, 2.0f, 0.1f);
		g_Camera->UpdateProjectionMatrix(glm::radians(45.f), (float)WIDTH / (float)HEIGHT);
		g_Camera->Update();

		g_Input->BindAction("TurnCamera", UserInput::EIT_OnHold, g_Camera, &cCamera::TurnCamera);
		g_Input->BindAction("ZoomCamera", UserInput::EIT_OnHold, g_Camera, &cCamera::ZoomCamera);

		g_Input->BindAxis("MoveRight", g_Camera, &cCamera::MoveRight);
		g_Input->BindAxis("MoveForward", g_Camera, &cCamera::MoveForward);
		g_Input->BindAxis("MoveUp", g_Camera, &cCamera::MoveUp);

	}
	void cleanupCamera()
	{
		safe_delete(g_Camera);
	}
}