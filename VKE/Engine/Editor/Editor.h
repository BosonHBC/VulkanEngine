#pragma once

struct ImGui_ImplVulkanH_Window;
struct GLFWwindow;
namespace VKE
{
	class VKRenderer;
	namespace Editor
	{
		/*
		* Window: Which glfw window this editor would be in
		* Renderer: Which vkRenderer this editor would use to render
		*/
		void Init(GLFWwindow* Window, VKRenderer* Renderer);
		void Update(VKRenderer* Renderer);
		void CleanUp(VKRenderer* Renderer);

		ImGui_ImplVulkanH_Window* GetMainWindowData();
	}
}