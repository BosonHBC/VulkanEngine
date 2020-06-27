#pragma once
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vector>
#include "Utilities.h"

namespace VKE
{
	class VKRenderer
	{
	public:
		VKRenderer() {};
		~VKRenderer() { window = nullptr; };

		VKRenderer(const VKRenderer& other) = delete;
		VKRenderer& operator =(const VKRenderer& other) = delete;

		int init(GLFWwindow* iWindow);
		void cleanUp();



	private:
		GLFWwindow* window;

		// Vulkan Components
		VkInstance vkInstance;
		VkQueue graphicQueue;

		struct
		{
			VkPhysicalDevice PD;		// Physical Device
			VkDevice LD;				// Logical Device
		} MainDevice;

		/** Create functions */
		void createInstance();
		void getPhysicalDevice();
		void createLogicalDevice();

		/** Support functions */
		bool checkInstanceExtensionSupport(std::vector<const char*>* checkExtentions);
		bool checkDeviceSuitable(const VkPhysicalDevice& device);
	
		/** Getters */
		FQueueFamilyIndices getQueueFamilies(const VkPhysicalDevice& device);
	};

}
