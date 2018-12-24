#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

#ifdef _WIN32
#include <Windows.h>
#include <vulkan/vulkan_win32.h>
#elif __linux__
#include <xcb/xcb.h>
#include <vulkan/vulkan_xcb.h>
#endif

int width, height;
VkInstance instance;
VkDebugUtilsMessengerEXT messenger;

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

static VKAPI_ATTR VkBool32 VKAPI_CALL messageCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	if(severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		printf("%s\n", pCallbackData->pMessage);
	return VK_FALSE;
}

void createWindow()
{
	width = 320;
	height = 240;
#ifdef _WIN32
	instanceHandle = GetModuleHandle(0);
	windowClass = (WNDCLASS) { CS_DBLCLKS, WindowProc, 0, 0, instanceHandle,
	  LoadIcon(0,IDI_APPLICATION), LoadCursor(0,IDC_ARROW),
	  CreateSolidBrush(COLOR_BACKGROUND), 0, "Engine" };
	RegisterClass(&windowClass);
	windowHandle = CreateWindow("Engine", "Vulkan", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, width, height,
		0, 0, instanceHandle, 0);
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
	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(xconn, 0, 16,"WM_DELETE_WINDOW");
	xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(xconn, cookie, 0);
	atom = reply->atom;
	xcb_change_property(xconn, XCB_PROP_MODE_REPLACE, window, protReply->atom, XCB_ATOM_ATOM, 32, 1, &reply->atom);
	xcb_change_property(xconn, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 7, "Vulkan");
	xcb_map_window(xconn, window);
	xcb_flush(xconn);
	printf("Created Window: Xorg XCB\n");
#endif
}

void createInstance()
{
	VkApplicationInfo appInfo = {0};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Engine";
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

#ifdef _WIN32
	const char const *PLATFORM_SURFACE_NAME = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#elif __linux__
	const char const *PLATFORM_SURFACE_NAME = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
#endif
	const char *extensionNames[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	  VK_KHR_SURFACE_EXTENSION_NAME, PLATFORM_SURFACE_NAME};
	size_t extensionCount = sizeof(extensionNames) / sizeof(extensionNames[0]);
	const char *layerNames[] = {"VK_LAYER_LUNARG_standard_validation"};
	size_t layerCount = sizeof(layerNames) / sizeof(layerNames[0]);
	
	VkInstanceCreateInfo instanceInfo = {0};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.enabledExtensionCount = extensionCount;
	instanceInfo.ppEnabledExtensionNames = extensionNames;
	instanceInfo.enabledLayerCount = layerCount;
	instanceInfo.ppEnabledLayerNames = layerNames;
	
	if(vkCreateInstance(&instanceInfo, NULL, &instance) == VK_SUCCESS)
		printf("Created Instance: LunarG\n");
}

void registerMessenger()
{
	VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {0};
	messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
	  VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
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

void setup()
{
	createWindow();
	createInstance();
	registerMessenger();
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
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
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

}

int main()
{
	setup();
	draw();
	clean();
}
