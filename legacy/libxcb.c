#include <xcb/xcb.h>

uint32_t width, height;
xcb_connection_t *xconn;
xcb_window_t window;
xcb_generic_event_t *event;
xcb_atom_t destroyEvent;

void createInstance()
{
	VkApplicationInfo appInfo = {0};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Engine";
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	appInfo.pEngineName = "Vulkan Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

	const char *layerNames[] = {"VK_LAYER_LUNARG_standard_validation"};
	uint32_t layerCount = sizeof(layerNames) / sizeof(layerNames[0]);
	const char *extensionNames[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	 VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME};
	uint32_t extensionCount = sizeof(extensionNames) / sizeof(extensionNames[0]);

	VkInstanceCreateInfo instanceInfo = {0};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.enabledLayerCount = layerCount;
	instanceInfo.ppEnabledLayerNames = layerNames;
	instanceInfo.enabledExtensionCount = extensionCount;
	instanceInfo.ppEnabledExtensionNames = extensionNames;

	vkCreateInstance(&instanceInfo, NULL, &instance);
}

void createWindow()
{
	xconn = xcb_connect(NULL, NULL);
	xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(xconn)).data;
	window = xcb_generate_id(xconn);

	uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	uint32_t values[] = {screen->black_pixel, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY};
	xcb_create_window(xconn, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, width, height, 0,
	 XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, mask, values);

	xcb_intern_atom_cookie_t protCookie = xcb_intern_atom(xconn, 0, 12, "WM_PROTOCOLS");
	xcb_intern_atom_reply_t* protReply = xcb_intern_atom_reply(xconn, protCookie, 0);
	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(xconn, 0, 16, "WM_DELETE_WINDOW");
	xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(xconn, cookie, 0);
	xcb_change_property(xconn, XCB_PROP_MODE_REPLACE, window, protReply->atom, XCB_ATOM_ATOM, 32, 1, &reply->atom);
	destroyEvent = reply->atom;

	xcb_map_window(xconn, window);
	xcb_flush(xconn);
}

void createSurface()
{
	VkXcbSurfaceCreateInfoKHR surfaceInfo = {0};
	surfaceInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	surfaceInfo.connection = xconn;
	surfaceInfo.window = window;
	vkCreateXcbSurfaceKHR(instance, &surfaceInfo, NULL, &surface);
}

int checkEvents()
{
	if(event = xcb_poll_for_event(xconn))
	{
		uint8_t response = event->response_type & ~0x80;

		if(response == XCB_CONFIGURE_NOTIFY)
		{
			xcb_configure_notify_event_t* configNotify = (xcb_configure_notify_event_t*)event;

			if(configNotify->width > 0 && configNotify->height > 0 &&
			 (configNotify->width != width || configNotify->height != height))
			{
				width = configNotify->width;
				height = configNotify->height;
			}
		}

		else if(response == XCB_CLIENT_MESSAGE)
			return ((xcb_client_message_event_t*)event)->data.data32[0]

		free(event);
	}

	return !destroyEvent;
}

void draw()
{
	while(checkEvents())
	{
		if(currentTime != timespec.tv_sec)
		{
			char title[18] = {0};
			sprintf(title, "%dx%d:%d", width, height, frameCount - checkPoint);
			xcb_change_property(xconn, XCB_PROP_MODE_REPLACE, window,
			 XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(title), title);
			xcb_flush(xconn);
			checkPoint = frameCount;
			currentTime = timespec.tv_sec;
		}
	}
}

void clean()
{
	xcb_destroy_window(xconn, window);
	xcb_disconnect(xconn);
	free(event);
}
