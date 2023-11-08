// Engines
#include "Engine.h"

#include "Graphics/VKRenderer.h"
#include "Input/UserInput.h"
#include "Camera.h"
#include "Editor/Editor.h"
#include "cTime.h"
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
	GLFWwindow* GWindow;
	VKRenderer* GRenderer;
	UserInput::FUserInput* GUserInput;
	cCamera* GCamera;

	//=================== Function declarations =================== 

	// GLFW
	void InitGLFW();
	void CleanupGLFW();
	// Input
	void InitInput();
	void window_focus_callback(GLFWwindow* window, int32 focused);
	void window_iconify_callback(GLFWwindow* window, int32 iconified);
	void CleanupInput();
	void QuitApp();

	// Camera
	void InitCamera();
	void CleanupCamera();

	int32 Init()
	{
		int32 result = EXIT_SUCCESS;
		InitGLFW();
		InitInput();
		InitCamera();
		
		GRenderer = DBG_NEW VKRenderer();
		if (GRenderer->init(GWindow) == EXIT_FAILURE)
		{
			return EXIT_FAILURE;
		}
		Editor::Init(GWindow, GRenderer);

		return result;
	}

	void Run()
	{
		if (!GWindow)
		{
			return;
		}

		double LastTime = glfwGetTime();
		while (!glfwWindowShouldClose(GWindow))
		{
			glfwPollEvents();

			double now = glfwGetTime();
			cTime::DT = now - LastTime;
			
			LastTime = now;
			// update editor
			Editor::Update(GRenderer);
			
			// not minimized
			if (!bWindowIconified && !Editor::GbMovingWindow)
			{
				GUserInput->Update();

				GCamera->Update();

				GRenderer->tick((float)cTime::DT);
				GRenderer->draw();
			}
		}
	}

	void Cleanup()
	{
		Editor::CleanUp(GRenderer);

		GRenderer->cleanUp();
		SAFE_DELETE(GRenderer)

		CleanupCamera();
		CleanupInput();
		CleanupGLFW();
	}


	GLFWwindow* GetGLFWWindow()
	{
		return GWindow;
	}

	cCamera* GetCurrentCamera()
	{
		return GCamera;
	}

	glm::ivec2 GetWindowExtent()
	{
		return glm::ivec2(WIDTH, HEIGHT);
	}

	glm::vec2 GetMouseDelta()
	{
		if (GUserInput)
		{
			return GUserInput->GetMouseDelta();
		}
		return glm::vec2(0.0f);
	}

	void InitGLFW()
	{
		glfwInit();

		// using Vulkan here
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

		GWindow = glfwCreateWindow(WIDTH, HEIGHT, WINDOW_NAME.c_str(), nullptr, nullptr);

		glfwSetWindowFocusCallback(GWindow, window_focus_callback);
		glfwSetWindowIconifyCallback(GWindow, window_iconify_callback);
	}
	void CleanupGLFW()
	{
		glfwDestroyWindow(GWindow);

		glfwTerminate();
	}

	void InitInput()
	{
		using namespace UserInput;
		GUserInput = DBG_NEW UserInput::FUserInput();
		GUserInput->AddActionKeyPairToMap("Escape", EKeyCode::Escape);
		GUserInput->BindAction("Escape", EIT_OnPressed, &QuitApp);

		GUserInput->AddActionKeyPairToMap("TurnCamera", EKeyCode::LMB);
		GUserInput->AddActionKeyPairToMap("ZoomCamera", EKeyCode::RMB);

		GUserInput->AddAxisKeyPairToMap("MoveRight", { EKeyCode::A,EKeyCode::D });
		GUserInput->AddAxisKeyPairToMap("MoveForward", { EKeyCode::S,EKeyCode::W });
		GUserInput->AddAxisKeyPairToMap("MoveUp", { EKeyCode::Control,EKeyCode::Space });

	}
	void window_focus_callback(GLFWwindow* window, int32 focused)
	{
		if (GUserInput)
		{
			GUserInput->bAppInFocus = focused;
		}
	}
	void window_iconify_callback(GLFWwindow* window, int32 iconified)
	{
		bWindowIconified = iconified > 0;
	}
	void QuitApp()
	{
		glfwSetWindowShouldClose(GWindow, true);
	}
	void CleanupInput()
	{
		SAFE_DELETE(GUserInput);
	}

	void InitCamera()
	{
		GCamera = DBG_NEW cCamera(glm::vec3(0, 1, 2.5), 15.f, 0.0f, 2.0f, 0.1f);
		GCamera->UpdateProjectionMatrix(glm::radians(45.f), (float)WIDTH / (float)HEIGHT);
		GCamera->Update();

		GUserInput->BindAction("TurnCamera", UserInput::EIT_OnHold, GCamera, &cCamera::TurnCamera);
		GUserInput->BindAction("ZoomCamera", UserInput::EIT_OnHold, GCamera, &cCamera::ZoomCamera);

		GUserInput->BindAxis("MoveRight", GCamera, &cCamera::MoveRight);
		GUserInput->BindAxis("MoveForward", GCamera, &cCamera::MoveForward);
		GUserInput->BindAxis("MoveUp", GCamera, &cCamera::MoveUp);

	}
	void CleanupCamera()
	{
		SAFE_DELETE(GCamera);
	}
}