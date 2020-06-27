#include "VKRenderer.h"
#include <stdexcept>
#include "stdlib.h"

namespace VKE
{

	int VKRenderer::init(GLFWwindow* iWindow)
	{
		window = iWindow;
		try
		{
			createInstance();
			getPhysicalDevice();
			createLogicalDevice();
		}
		catch (const std::runtime_error &e)
		{
			printf("ERROR: %s \n", e.what());
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	void VKRenderer::cleanUp()
	{
		vkDestroyDevice(MainDevice.LD, nullptr);
		vkDestroyInstance(vkInstance, nullptr);
	}

	void VKRenderer::createInstance()
	{
		// Create a application info
		// Most data here doesn't affect the program and is for developer convenience
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Vulkan App";					// Application name
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);		// custom application version
		appInfo.pEngineName = "VKE";								// Custom engine name
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);			// Custom engine version
		appInfo.apiVersion = VK_API_VERSION_1_2;					// Vulkan version

		// Create information for a VKInstance
		VkInstanceCreateInfo CreateInfo = {};

		CreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;	// VK_STRUCTURE_TYPE + xxxInfo that you want, in this case: InstanceCreateInfo
		CreateInfo.pNext = nullptr;									// this will point to extended information, can be another struct
		/*CreateInfo.flags = VK_WHATEVER | VK_OTHER;*/
		CreateInfo.pApplicationInfo = &appInfo;


		// Create list to hold instance extensions
		std::vector<const char*> InstanceExtensions;
		uint32_t glfwExtensionCount = 0;							// GLFW may require multiple extensions;
		const char** glfwExtensions;								// Extensions passed as array of c-strings, so need the array to pointer

		// Get glfw extensions
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		// Check instance Extensions supported
		if (!checkInstanceExtensionSupport(&InstanceExtensions))
		{
			throw std::runtime_error("VKInstance does not support required extensions!");
		}

		CreateInfo.enabledExtensionCount = glfwExtensionCount;
		CreateInfo.ppEnabledExtensionNames = glfwExtensions;

		CreateInfo.enabledLayerCount = 0;							// validation layer count
		CreateInfo.ppEnabledLayerNames = nullptr;					// @TODO: Setup validation layers that instance will use

		// VkAllocationCallbacks is a custom allocator, not using here
		auto result = vkCreateInstance(&CreateInfo, nullptr, &vkInstance);
		if (result != VkResult::VK_SUCCESS)
		{
			char ErrorMsg[100];
			sprintf_s(ErrorMsg, "Fail to Create VKInstance, exit code: %d\n", result);
			throw std::runtime_error(ErrorMsg);
		}
	}

	void VKRenderer::getPhysicalDevice()
	{
		uint32_t DeviceCount = 0;
		vkEnumeratePhysicalDevices(vkInstance, &DeviceCount, nullptr);

		printf("Physical Device Count: %i\n", DeviceCount);
		// if no devices available, Vulkan not supported
		if (DeviceCount == 0)
		{
			throw std::runtime_error("Cannot find any GPU support Vulkan Instance");
		}

		std::vector<VkPhysicalDevice> devices(DeviceCount);
		vkEnumeratePhysicalDevices(vkInstance, &DeviceCount, devices.data());
		
		// Pick first device first
		for (const auto& device : devices)
		{
			if (checkDeviceSuitable(device))
			{
				MainDevice.PD = device;
				break;
			}
			
		}
	}

	void VKRenderer::createLogicalDevice()
	{
		// Get the queue family indices for the chosen PD
		FQueueFamilyIndices indieces = getQueueFamilies(MainDevice.PD);
		
		// Queue is needed to create a logical device, now only 1 is needed, will add more.
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = indieces.graphicFamily;						// Index of the family to create a queue from, this is a graphic family
		queueCreateInfo.queueCount = 1;
		float priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority;									// If there are multiple queues, GPU needs to know the execution order of different queues (1.0 == highest prioity)

		// Info to create a (logical) device
		VkDeviceCreateInfo DeviceCreateInfo = {};
		DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		DeviceCreateInfo.queueCreateInfoCount = 1;										// Number of queues in this device.
		DeviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;							// list of queue create infos so that the devices will create required queues.
		DeviceCreateInfo.enabledExtensionCount = 0;										// Number of enabled logical device extensions
		DeviceCreateInfo.ppEnabledExtensionNames = nullptr;								// List of logical device extensions

		// Physical Device Features the Logical Device will use
		VkPhysicalDeviceFeatures PDFeatures = {};

		DeviceCreateInfo.pEnabledFeatures = &PDFeatures;
	
		VkResult result = vkCreateDevice(MainDevice.PD, &DeviceCreateInfo, nullptr, &MainDevice.LD);
		if (result != VkResult::VK_SUCCESS)
		{
			char ErrorMsg[100];
			sprintf_s(ErrorMsg, "Fail to Create VKLogical Device, exit code: %d\n", result);
			throw std::runtime_error(ErrorMsg);
		}

		// After LD is created, queues should be created too, Save the queues
		vkGetDeviceQueue(
			/*From given LD*/MainDevice.LD, 
			/*given queue family*/ indieces.graphicFamily,
			/*given queue index(0 only one queue)*/ 0, 
			/*Out queue*/ &graphicQueue);
	}

	bool VKRenderer::checkInstanceExtensionSupport(std::vector<const char*>* checkExtentions)
	{
		// Need to get number of extensions to create array of correct size to hold extensions
		uint32_t ExtensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, nullptr);
		
		printf("Extension Count: %i\n", ExtensionCount);

		std::vector<VkExtensionProperties> extensions(ExtensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, extensions.data());

		// Check if given extensions are available
		for (const auto& checkExtension : *checkExtentions)
		{
			bool hasExtension = false;
			for (const auto& extension : extensions)
			{
				if (strcmp(checkExtension, extension.extensionName))
				{
					hasExtension = true;
					break;
				}
			}
			if (!hasExtension)
			{
				return false;
			}
		}
		return true;
	}

	bool VKRenderer::checkDeviceSuitable(const VkPhysicalDevice& device)
	{
		// Information about the device itself (ID, name, type, vendor, etc)
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
	
		// Information about what the device supports (geo shader, tess sahder, wide lines, etc)
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		FQueueFamilyIndices indices = getQueueFamilies(device);

		return indices.IsValid();
	}

	FQueueFamilyIndices VKRenderer::getQueueFamilies(const VkPhysicalDevice& device)
	{
		FQueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

		// Go through each queue family and check if it has at least 1 of the required types of queue
		int i = 0;
		for (const auto& queueFamily : queueFamilyList)
		{
			// At least one queue and it has graphic queue family (a queue could be multiple types)
			if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
			{
				indices.graphicFamily = i;
				break;
			}
			++i;
		}

		return indices;
	}

}