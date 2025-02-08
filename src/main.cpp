#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
// #include <vulkan/vulkan.h> // automatically included by GLFW

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <set>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// Proxy function to pass the debug messenger creation function to vkCreateInstance since it's an extension function and thus not automatically loaded
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	// Will return a function pointer to the function that creates the debug messenger or nullptr if it doesn't exist
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

struct QueueFamilyIndices {
	// std::optional is a wrapper that can hold a value or nothing. it contains no value until you assign something to it.
	// You can check if it contains a value with has_value() and get the value with value()
	std::optional<uint32_t> graphicsFamily;
	// We have to check for a distinct present queue family because the graphics queue family may not support presentation
	std::optional<uint32_t> presentFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}

};

class HelloTriangleApplication {
public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}
private:
	GLFWwindow* window;

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;

	VkSurfaceKHR surface;

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;

	VkQueue graphicsQueue;
	VkQueue presentQueue;

	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't create OpenGL context
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // disable window resizing

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

		if (!window) {
			throw std::runtime_error("Failed to create GLFW window!");
		}
	}

	void initVulkan() {
		createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
	}

	void mainLoop() {
		// Keep running until the window is closed or error occurs
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
		}
	}

	void cleanup() {
		vkDestroyDevice(device, nullptr);

		if (enableValidationLayers) {
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}

		vkDestroySurfaceKHR(instance, surface, nullptr);

		vkDestroyInstance(instance, nullptr);

		glfwDestroyWindow(window);

		glfwTerminate();
	}

	void createInstance() {
		// ===== Check validation layer support =====
		if (enableValidationLayers && !checkValidationLayerSupport()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}

		// ===== Create the Vulkan instance =====
		// Create a struct to store information about the application
		// This is optional, but may provide some useful information to the driver to optimize for the application
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		// A lot of info in Vulkan is passed through structs instead of function parameters
		// This struct is not optional and tells the Vulkan driver which global extensions and validation layers to use
		VkInstanceCreateInfo createInfo{};
#ifdef __APPLE__
		// ✅ Enable portability enumeration for MoltenVK on macOS
		createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		// Since Vulkan is a platform agnostic API, we need an extension to interface with the window system
		// GLFW has a function to get the extensions needed to interface with Vulkan
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			// Allow the debug messenger to be created for the instance
			populateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else {
			createInfo.enabledLayerCount = 0;

			createInfo.pNext = nullptr;
		}

		// Create the instance
		// Note that this follows the general pattern of Vulkan object creation where we pass in:
		// a pointer to a struct with creation info, a pointer to a custom allocator callback, and a pointer to a variable to store the handle in
		// Check if the instance was created successfully
		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}
	}

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		// We can filter which severity types we want to receive messages for
		createInfo.messageSeverity = // not including info bit because it's just general debugging info
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		// We can also filter which types of messages the callback is notified about
		createInfo.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		// The pfnUserCallback member is the pointer to the callback function
		createInfo.pfnUserCallback = debugCallback;
		// The pUserData member is a pointer that is passed to the callback function
		createInfo.pUserData = nullptr; // Optional
	}

	void setupDebugMessenger() {
		if (!enableValidationLayers) return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		populateDebugMessengerCreateInfo(createInfo);

		// Create the debug messenger
		if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
			throw std::runtime_error("failed to set up debug messenger!");
		}
	}

	void createSurface() {
		// glfwCreateWindowSurface is a platform agnostic function that creates a surface for Vulkan to interface with the window system
		// Thus, we don't need to worry about platform specific code
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}
	}

	void pickPhysicalDevice() {
		// Get the number of physical devices
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		if (deviceCount == 0) {
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		for (const auto& device : devices) {
			if (isDeviceSuitable(device)) {
				physicalDevice = device;
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("failed to find a suitable GPU!");
		}
	}

	void createLogicalDevice() {
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		float queuePriority = 1.0f;
		// Create a queue for each unique queue family
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			// Only need one queue since we can create all of the command buffers on multiple threads and then submit them all at once on the main thread
			queueCreateInfo.queueCount = 1;
			// The priority of the queue (from 0.0 to 1.0)
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		// Specify the set of device features that we'll be using
		VkPhysicalDeviceFeatures deviceFeatures{};

		// Create the logical device
		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();

		createInfo.pEnabledFeatures = &deviceFeatures;

		// Modern Vulkan doesn't make a distinction between instance and device specific validation layers
		// That means enabledLayerCount and ppEnabledLayerNames are deprecated, but it's still a good idea to set them to be compatible with older implementations
		createInfo.enabledExtensionCount = 0;
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		// Instantiate the logical device
		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
			throw std::runtime_error("failed to create logical device!");
		}

		// Retrieve the handle of the queue that was created along with the logical device
		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
		vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
	}

	bool isDeviceSuitable(VkPhysicalDevice device) {
		//// Get basic properties of the device
		//VkPhysicalDeviceProperties deviceProperties;
		//vkGetPhysicalDeviceProperties(device, &deviceProperties);
		//// Get optional features of the device (like texture compression, 64-bit floats, multi-viewport rendering, etc.)
		//VkPhysicalDeviceFeatures deviceFeatures;
		//vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		//return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader;

		QueueFamilyIndices indices = findQueueFamilies(device);
		return indices.isComplete();
	}

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
		QueueFamilyIndices indices;

		// Get list of queue families and their properties
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
		// queueFamilies stores the structs with the properties of each queue family,
		// including the type of operations that are supported and the number of queues that can be created based on it
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		// Find a queue family that supports VK_QUEUE_GRAPHICS_BIT
		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphicsFamily = i;
			}

			// Look for a queue family that has the capability of presenting to our window surface
			// This could be the same queue as the one that supports graphics operations, but it could also be different
			// Therefore, we will treat them as distinct queue families
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
			if (presentSupport) {
				indices.presentFamily = i;
			}

			if (indices.isComplete()) {
				break;
			}

			i++;
		}

		return indices;
	}

	bool checkValidationLayerSupport() {
		// Get the number of available layers
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		// Check if all of the layers in validationLayers are available
		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}

	// Handle the validation layers callback to decide how to handle the validation message
	std::vector<const char*> getRequiredExtensions() {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
#ifdef __APPLE__
		// ✅ Add MoltenVK portability extension for macOS
		extensions.push_back("VK_KHR_portability_enumeration");
#endif

		return extensions;
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, // diagnostic, info, warning, error
		VkDebugUtilsMessageTypeFlagsEXT messageType, // general, validation (violates specification or possible mistake), performance (potential non-optimal Vulkan use)
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, // struct with details of the message (like message, objects, and object count)
		void* pUserData // pointer to data specified during callback creation
	) {
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}
};

int main() {
	HelloTriangleApplication app;

	try {
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
