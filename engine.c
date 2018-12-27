#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

#ifndef NDEBUG
	#include "support.h"
#endif
#ifdef _WIN32
	#include <windows.h>
	#include <vulkan/vulkan_win32.h>
#elif __linux__
	#include <xcb/xcb.h>
	#include <vulkan/vulkan_xcb.h>
#endif

struct swapchainDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	uint32_t formatCount, modeCount;
	VkSurfaceFormatKHR* surfaceFormats;
	VkPresentModeKHR* presentModes;
} typedef SwapchainDetails;

int width, height;
VkInstance instance;
VkSurfaceKHR surface;
VkPhysicalDevice physicalDevice;

#ifndef NDEBUG
	VkDebugUtilsMessengerEXT messenger;
	static VKAPI_ATTR VkBool32 VKAPI_CALL messageCallback(
	  VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
	  const VkDebugUtilsMessengerCallbackDataEXT*, void*);
#endif

#ifdef _WIN32
	HINSTANCE instanceHandle;
	WNDCLASS windowClass;
	HWND windowHandle;
	LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
#elif __linux__
	xcb_connection_t *xconn;
	xcb_window_t window;
	xcb_generic_event_t *event;
	xcb_atom_t atom;
#endif

void createInstance()
{
	VkApplicationInfo appInfo = {0};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Engine";
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	appInfo.pEngineName = "Mungui Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

	#ifndef NDEBUG
		const char *layerNames[] = {"VK_LAYER_LUNARG_standard_validation"};
		uint32_t layerCount = sizeof(layerNames) / sizeof(layerNames[0]);
	#else
		const char *layerNames[] = NULL;
		uint32_t layerCount = 0;
	#endif

	#ifdef _WIN32
		const char *PLATFORM_SURFACE_NAME = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
	#elif __linux__
		const char *PLATFORM_SURFACE_NAME = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
	#endif

	const char *extensionNames[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	  VK_KHR_SURFACE_EXTENSION_NAME, PLATFORM_SURFACE_NAME};
//	const char *extensionNames[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
//	  VK_KHR_SURFACE_EXTENSION_NAME};
	uint32_t extensionCount = sizeof(extensionNames) / sizeof(extensionNames[0]);
	
	VkInstanceCreateInfo instanceInfo = {0};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.enabledLayerCount = layerCount;
	instanceInfo.ppEnabledLayerNames = layerNames;
	instanceInfo.enabledExtensionCount = extensionCount;
	instanceInfo.ppEnabledExtensionNames = extensionNames;
	
	if(vkCreateInstance(&instanceInfo, NULL, &instance) == VK_SUCCESS)
		printf("Created Instance: LunarG\n");
}

#ifndef NDEBUG
	static VKAPI_ATTR VkBool32 VKAPI_CALL messageCallback(
	  VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
	  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
			printf("%s\n", pCallbackData->pMessage);
		return VK_FALSE;
	}

	void registerMessenger()
	{
		VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {0};
		messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		messengerInfo.messageSeverity = //VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		  //VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		messengerInfo.pfnUserCallback = messageCallback;
		messengerInfo.pUserData = NULL;

		PFN_vkCreateDebugUtilsMessengerEXT createMessenger =
		  (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (createMessenger != NULL && createMessenger(instance, &messengerInfo, NULL, &messenger) == VK_SUCCESS)
			printf("Registered Messenger: Validation Layers\n");
	}
#endif

void createWindow()
{
	width = 320;
	height = 240;
	#ifdef _WIN32
		instanceHandle = GetModuleHandle(0);
		windowClass = (WNDCLASS) {CS_DBLCLKS, WindowProc, 0, 0, instanceHandle,
		  LoadIcon(0, IDI_APPLICATION), LoadCursor(0, IDC_ARROW),
		  CreateSolidBrush(COLOR_BACKGROUND), 0, "Engine"
		};
		RegisterClass(&windowClass);
		windowHandle = CreateWindow("Engine", "Vulkan", WS_OVERLAPPEDWINDOW,
		  CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, instanceHandle, 0);
		ShowWindow(windowHandle, SW_SHOWDEFAULT);
		UpdateWindow(windowHandle);
		printf("Created Window: Win32 API\n");
	#elif __linux__
		xconn = xcb_connect(NULL, NULL);
		xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(xconn)).data;
		window = xcb_generate_id(xconn);
		xcb_create_window(xconn, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, width, height, 0,
		  XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, XCB_CW_EVENT_MASK,
		  &(uint32_t) {XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_RESIZE_REDIRECT});
		xcb_intern_atom_cookie_t protCookie = xcb_intern_atom(xconn, 1, 12, "WM_PROTOCOLS");
		xcb_intern_atom_reply_t* protReply = xcb_intern_atom_reply(xconn, protCookie, 0);
		xcb_intern_atom_cookie_t cookie = xcb_intern_atom(xconn, 0, 16, "WM_DELETE_WINDOW");
		xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(xconn, cookie, 0);
		atom = reply->atom;
		xcb_change_property(xconn, XCB_PROP_MODE_REPLACE, window, protReply->atom, XCB_ATOM_ATOM, 32, 1, &reply->atom);
		xcb_change_property(xconn, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 7, "Vulkan");
		xcb_map_window(xconn, window);
		xcb_flush(xconn);
		printf("Created Window: Xorg XCB\n");
	#endif
}

void createSurface()
{
	#ifdef _WIN32
		VkWin32SurfaceCreateInfoKHR createInfo = {0};
		createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		createInfo.hwnd = windowHandle;
		createInfo.hinstance = instanceHandle;
		if (vkCreateWin32SurfaceKHR(instance, &createInfo, NULL, &surface) == VK_SUCCESS)
			printf("Created Surface: Win32 Surface\n");
	#elif __linux__
		VkXcbSurfaceCreateInfoKHR surfaceInfo = {0};
		surfaceInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
		surfaceInfo.connection = xconn;
		surfaceInfo.window = window;
		if (vkCreateXcbSurfaceKHR(instance, &surfaceInfo, NULL, &surface) == VK_SUCCESS)
			printf("Created Surface: XCB Surface\n");
	#endif
}

void pickPhysicalDevice() //TODO: Generalize the picking
{
	physicalDevice = VK_NULL_HANDLE;

	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);

	printf("Found: %d\n", deviceCount);
	VkPhysicalDevice *devices = malloc(deviceCount * sizeof(VkPhysicalDevice));
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices);

	for (uint32_t deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++)
	{
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(devices[deviceIndex], &deviceProperties);
		printf("%s\n", deviceProperties.deviceName);

		//VkPhysicalDeviceFeatures deviceFeatures;
		//vkGetPhysicalDeviceFeatures(devices[deviceIndex], &deviceFeatures);
		/*
		uint32_t extensionCount, extensionExists = 0;
		vkEnumerateDeviceExtensionProperties(devices[deviceIndex], NULL, &extensionCount, NULL);
		VkExtensionProperties *extensionProperties = malloc(extensionCount * sizeof(VkExtensionProperties));
		vkEnumerateDeviceExtensionProperties(devices[deviceIndex], NULL, &extensionCount, extensionProperties);
		for (uint32_t extensionIndex = 0; extensionIndex < extensionCount; extensionIndex++)
			if (!strcmp(extensionProperties[extensionIndex].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
				extensionExists = 1;
		
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devices[deviceIndex], surface, &details.capabilities);
		vkGetPhysicalDeviceSurfaceFormatsKHR(devices[deviceIndex], surface, &details.formatCount, NULL);
		vkGetPhysicalDeviceSurfacePresentModesKHR(devices[deviceIndex], surface, &details.modeCount, NULL);
		details.surfaceFormats = malloc(details.formatCount * sizeof(VkSurfaceFormatKHR));
		details.presentModes = malloc(details.modeCount * sizeof(VkPresentModeKHR));
		vkGetPhysicalDeviceSurfaceFormatsKHR(devices[deviceIndex], surface, &details.formatCount, details.surfaceFormats);
		vkGetPhysicalDeviceSurfacePresentModesKHR(devices[deviceIndex], surface, &details.modeCount, details.presentModes);

		if (extensionExists && details.formatCount && details.modeCount && deviceFeatures.geometryShader
			&& deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			physicalDevice = devices[deviceIndex];
			printf("Picked Physical Device! (Index: %d)\n", deviceIndex);
			break;
		}
		else
		{
			details.capabilities = (VkSurfaceCapabilitiesKHR) { 0 };
			details.formatCount = 0;
			details.modeCount = 0;
			free(details.surfaceFormats);
			free(details.presentModes);
		}*/
	}
}

void setup()
{
	createInstance();
	#ifndef NDEBUG
//		registerMessenger();
	#endif
//	createWindow();
//	createSurface();
	pickPhysicalDevice();
}

#ifdef _WIN32
	LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (message == WM_DESTROY)
		{
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
#endif

void draw()
{
	#ifdef _WIN32
		MSG msg;
		while (1)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				if (msg.message == WM_QUIT)
					break;
			}
		}
	#elif __linux__
		while(1)
		{
			event = xcb_poll_for_event(xconn);
			if(event)
			{
				if((event->response_type & ~0x80) == XCB_RESIZE_REQUEST)
				{
					width = ((xcb_resize_request_event_t*)event)->width;
					height = ((xcb_resize_request_event_t*)event)->height;
					continue;
				}
				else if((event->response_type & ~0x80) == XCB_CLIENT_MESSAGE &&
				  ((xcb_client_message_event_t*)event)->data.data32[0] == atom)
					break;
				free(event);
			}
		}
		free(event);
	#endif
}

void clean()
{
/*	vkDestroySurfaceKHR(instance, surface, NULL);
	#ifdef __linux__
		free(event);
		xcb_destroy_window(xconn, window);
		xcb_disconnect(xconn);
	#endif
	#ifndef NDEBUG
		PFN_vkDestroyDebugUtilsMessengerEXT destroyMessenger =
		  (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (destroyMessenger != NULL)
			destroyMessenger(instance, messenger, NULL);
	#endif*/
	vkDestroyInstance(instance, NULL);
}

int main()
{
	setup();
//	draw();
	clean();
//	getch();
}
