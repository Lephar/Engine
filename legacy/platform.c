#ifdef _WIN32
	#include <windows.h>
	#include <vulkan/vulkan_win32.h>
#elif __linux__
	#include <xcb/xcb.h>
	#include <vulkan/vulkan_xcb.h>
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

#ifndef NDEBUG
	VkDebugUtilsMessengerEXT messenger;
	static VKAPI_ATTR VkBool32 VKAPI_CALL messageCallback(
	  VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
	  const VkDebugUtilsMessengerCallbackDataEXT*, void*);
#endif

	#ifdef _WIN32
		const char *PLATFORM_SURFACE_NAME = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
	#elif __linux__
		const char *PLATFORM_SURFACE_NAME = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
	#endif
	
	#ifndef NDEBUG
		const char *layerNames[] = {"VK_LAYER_LUNARG_standard_validation"};
		uint32_t layerCount = sizeof(layerNames) / sizeof(layerNames[0]);
	#else
		const char **layerNames = NULL;
		uint32_t layerCount = 0;
	#endif

#ifndef NDEBUG
	static VKAPI_ATTR VkBool32 VKAPI_CALL messageCallback(
	  VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
	  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		if(severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
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
		if(createMessenger != NULL && createMessenger(instance, &messengerInfo, NULL, &messenger) == VK_SUCCESS)
			printf("Registered Messenger: Validation Layers\n");
	}
#endif

	#ifdef _WIN32
		height += 40;
		instanceHandle = GetModuleHandle(0);
		windowClass = (WNDCLASS){CS_DBLCLKS, WindowProc, 0, 0, instanceHandle,
		  LoadIcon(0, IDI_APPLICATION), LoadCursor(0, IDC_ARROW),
		  CreateSolidBrush(COLOR_BACKGROUND), 0, "Engine"};
		RegisterClass(&windowClass);
		windowHandle = CreateWindow("Engine", "Vulkan", WS_OVERLAPPEDWINDOW,
		  CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, instanceHandle, 0);
		ShowWindow(windowHandle, SW_SHOWDEFAULT);
		UpdateWindow(windowHandle);
		RECT rect;
		GetClientRect(windowHandle, &rect);
		width = rect.right;
		height = rect.bottom;
		printf("Created Window: Win32 API\n");
	#elif __linux__
		xconn = xcb_connect(NULL, NULL);
		xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(xconn)).data;
		window = xcb_generate_id(xconn);
		uint32_t valueList[] = {screen->black_pixel,
		  XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_STRUCTURE_NOTIFY};
		xcb_create_window(xconn, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, width, height, 0,
		  XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, valueList);
		const char *message = "WM_PROTOCOLS";
		xcb_intern_atom_cookie_t protCookie = xcb_intern_atom(xconn, 0, strlen(message), message);
		xcb_intern_atom_reply_t* protReply = xcb_intern_atom_reply(xconn, protCookie, 0);
		message = "WM_DELETE_WINDOW";
		xcb_intern_atom_cookie_t cookie = xcb_intern_atom(xconn, 0, strlen(message), message);
		xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(xconn, cookie, 0);
		atom = reply->atom;
		message = "Vulkan";
		xcb_change_property(xconn, XCB_PROP_MODE_REPLACE, window, protReply->atom, XCB_ATOM_ATOM,
		  32, 1, &reply->atom);
		xcb_change_property(xconn, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
		  8, strlen(message), message);
		xcb_map_window(xconn, window);
		xcb_flush(xconn);
		printf("Created Window: Xorg XCB\n");
	#endif

	#ifdef _WIN32
		VkWin32SurfaceCreateInfoKHR surfaceInfo = {0};
		surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceInfo.hwnd = windowHandle;
		surfaceInfo.hinstance = instanceHandle;
		if(vkCreateWin32SurfaceKHR(instance, &surfaceInfo, NULL, &surface) == VK_SUCCESS)
			printf("Created Surface: Win32 Surface\n");
	#elif __linux__
		VkXcbSurfaceCreateInfoKHR surfaceInfo = {0};
		surfaceInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
		surfaceInfo.connection = xconn;
		surfaceInfo.window = window;
		if(vkCreateXcbSurfaceKHR(instance, &surfaceInfo, NULL, &surface) == VK_SUCCESS)
			printf("Created Surface: XCB Surface\n");
	#endif

	#ifndef DEBUG
		const char *layerNames[] = {"VK_LAYER_LUNARG_standard_validation"};
		uint32_t layerCount = sizeof(layerNames) / sizeof(layerNames[0]);
	#else
		const char **layerNames = NULL;
		uint32_t layerCount = 0;
	#endif

	#ifndef NDEBUG
		registerMessenger();
	#endif

#ifdef _WIN32
	LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if(message == WM_DESTROY)
		{
			PostQuitMessage(0);
			return 0;
		}
		else if(message == WM_SIZING)
		{
			RECT rect;
			GetClientRect(hWnd, &rect);
			if(rect.right > 0 && rect.bottom > 0 && (rect.right != width || rect.bottom != height))
			{
				width = rect.right;
				height = rect.bottom;
				recreateSwapchain();
			}
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
#endif

		#ifdef _WIN32
			MSG msg;
			if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				if(msg.message == WM_QUIT)
					break;
			}
		#elif __linux__
			event = xcb_poll_for_event(xconn);
			if(event)
			{
				if((event->response_type & ~0x80) == XCB_CONFIGURE_NOTIFY)
				{
					xcb_configure_notify_event_t* configNotify = (xcb_configure_notify_event_t*)event;
					if(configNotify->width > 0 && configNotify->height > 0 &&
					  (configNotify->width != width || configNotify->height != height))
					{
						width = configNotify->width;
						height = configNotify->height;
						recreateSwapchain();
					}
				}
				else if((event->response_type & ~0x80) == XCB_CLIENT_MESSAGE &&
				  ((xcb_client_message_event_t*)event)->data.data32[0] == atom)
					break;
				free(event);
			}
		#endif

	#ifdef __linux__
		xcb_destroy_window(xconn, window);
		xcb_disconnect(xconn);
	#endif
	#ifndef NDEBUG
		PFN_vkDestroyDebugUtilsMessengerEXT destroyMessenger =
		  (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if(destroyMessenger != NULL)
			destroyMessenger(instance, messenger, NULL);
	#endif

