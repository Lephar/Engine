#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <vulkan/vulkan.h>

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
SwapchainDetails swapchainDetails;
VkDevice device;
uint32_t presentIndex, graphicsIndex;
VkQueue presentQueue, graphicsQueue;
uint32_t framebufferSize, framebufferLimit;
VkSwapchainKHR swapchain;
VkFormat swapchainFormat;
VkExtent2D swapchainExtent;
VkImage *swapchainImages;
VkImageView *swapchainViews;
VkRenderPass renderPass;
VkPipelineLayout pipelineLayout;
VkPipeline graphicsPipeline;
VkFramebuffer *swapchainFramebuffers;
VkCommandPool commandPool;
VkCommandBuffer *commandBuffers;
VkSemaphore *imageAvailable, *renderFinished;
VkFence *frameFences;

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
		const char **layerNames = NULL;
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
		height += 40;
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
		RECT rect;
		GetClientRect(windowHandle, &rect);
		width = rect.right;
		height = rect.bottom;
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

SwapchainDetails generateSwapchainDetails(VkPhysicalDevice temporaryDevice)
{
	SwapchainDetails temporaryDetails = {0};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(temporaryDevice, surface, &temporaryDetails.capabilities);
	vkGetPhysicalDeviceSurfaceFormatsKHR(temporaryDevice, surface, &temporaryDetails.formatCount, NULL);
	vkGetPhysicalDeviceSurfacePresentModesKHR(temporaryDevice, surface, &temporaryDetails.modeCount, NULL);
	temporaryDetails.surfaceFormats = malloc(temporaryDetails.formatCount * sizeof(VkSurfaceFormatKHR));
	temporaryDetails.presentModes = malloc(temporaryDetails.modeCount * sizeof(VkPresentModeKHR));
	vkGetPhysicalDeviceSurfaceFormatsKHR(temporaryDevice, surface, &temporaryDetails.formatCount, temporaryDetails.surfaceFormats);
	vkGetPhysicalDeviceSurfacePresentModesKHR(temporaryDevice, surface, &temporaryDetails.modeCount, temporaryDetails.presentModes);
	return temporaryDetails;
}

void pickPhysicalDevice()
{
	physicalDevice = VK_NULL_HANDLE;

	int32_t maxScore = -1, bestIndex = -1;
	uint32_t deviceCount;
	char *deviceName = malloc(VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);
	vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);
	VkPhysicalDevice *devices = malloc(deviceCount * sizeof(VkPhysicalDevice));
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices);

	for (uint32_t deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++)
	{
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		uint32_t extensionCount, formatCount, modeCount, swapchainSupport = 0;

		vkGetPhysicalDeviceProperties(devices[deviceIndex], &deviceProperties);
		vkGetPhysicalDeviceFeatures(devices[deviceIndex], &deviceFeatures);
		vkGetPhysicalDeviceSurfaceFormatsKHR(devices[deviceIndex], surface, &formatCount, NULL);
		vkGetPhysicalDeviceSurfacePresentModesKHR(devices[deviceIndex], surface, &modeCount, NULL);

		vkEnumerateDeviceExtensionProperties(devices[deviceIndex], NULL, &extensionCount, NULL);
		VkExtensionProperties *extensionProperties = malloc(extensionCount * sizeof(VkExtensionProperties));
		vkEnumerateDeviceExtensionProperties(devices[deviceIndex], NULL, &extensionCount, extensionProperties);
		for (uint32_t extensionIndex = 0; extensionIndex < extensionCount; extensionIndex++)
			if (swapchainSupport = !strcmp(extensionProperties[extensionIndex].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
				break;
		free(extensionProperties);

		if (formatCount && modeCount && swapchainSupport && deviceFeatures.geometryShader)
		{
			int32_t deviceScore = extensionCount + (formatCount + modeCount) * 16;
			if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				deviceScore *= 2;

			if (deviceScore >= maxScore)
			{
				maxScore = deviceScore;
				bestIndex = deviceIndex;
				strncpy(deviceName, deviceProperties.deviceName, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);
			}
		}
	}

	if (maxScore != -1)
	{
		physicalDevice = devices[bestIndex];
		swapchainDetails = generateSwapchainDetails(physicalDevice);
		printf("Picked Physical Device: %s\n", deviceName);
	}

	free(deviceName);
	free(devices);
}

void createLogicalDevice()
{
	graphicsIndex = presentIndex = -1;
	graphicsQueue = presentQueue = VK_NULL_HANDLE;

	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, NULL);
	VkQueueFamilyProperties *queueProperties = malloc(queueCount * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProperties);

	for (uint32_t queueIndex = 0; queueIndex < queueCount; queueIndex++)
	{
		VkBool32 presentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueIndex, surface, &presentSupport);
		if (queueProperties[queueIndex].queueCount > 0 && presentSupport)
			presentIndex = queueIndex;
		if (queueProperties[queueIndex].queueCount > 0 && queueProperties[queueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			graphicsIndex = queueIndex;
	}

	float queuePriority = 1.0f;
	queueCount = graphicsIndex == presentIndex ? 1 : 2; 
	VkDeviceQueueCreateInfo *queueList = malloc(queueCount * sizeof(VkDeviceQueueCreateInfo));

	VkDeviceQueueCreateInfo graphicsInfo = { 0 };
	graphicsInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	graphicsInfo.queueCount = 1;
	graphicsInfo.queueFamilyIndex = graphicsIndex;
	graphicsInfo.pQueuePriorities = &queuePriority;
	queueList[0] = graphicsInfo;
	
	if(queueCount == 2)
	{
		VkDeviceQueueCreateInfo presentInfo = { 0 };
		presentInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		presentInfo.queueCount = 1;
		presentInfo.queueFamilyIndex = presentIndex;
		presentInfo.pQueuePriorities = &queuePriority;
		queueList[1] = presentInfo;
	}

	VkPhysicalDeviceFeatures deviceFeatures = { 0 };

	#ifndef DEBUG
		const char *layerNames[] = {"VK_LAYER_LUNARG_standard_validation"};
		uint32_t layerCount = sizeof(layerNames) / sizeof(layerNames[0]);
	#else
		const char **layerNames = NULL;
		uint32_t layerCount = 0;
	#endif

	const char *extensionNames[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	uint32_t extensionCount = sizeof(extensionNames) / sizeof(extensionNames[0]);

	VkDeviceCreateInfo deviceInfo = { 0 };
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.enabledLayerCount = layerCount;
	deviceInfo.ppEnabledLayerNames = layerNames;
	deviceInfo.enabledExtensionCount = extensionCount;
	deviceInfo.ppEnabledExtensionNames = extensionNames;
	deviceInfo.pEnabledFeatures = &deviceFeatures;
	deviceInfo.queueCreateInfoCount = queueCount;
	deviceInfo.pQueueCreateInfos = queueList;
	
	if (vkCreateDevice(physicalDevice, &deviceInfo, NULL, &device) == VK_SUCCESS)
		printf("Created Logical Device: Queue Count = %d\n", deviceInfo.queueCreateInfoCount);
	vkGetDeviceQueue(device, graphicsIndex, 0, &graphicsQueue);
	if (graphicsIndex != -1 && graphicsQueue != VK_NULL_HANDLE)
		printf("Acquired Graphics Queue Handle: Index = %d\n", graphicsIndex);
	vkGetDeviceQueue(device, presentIndex, 0, &presentQueue);
	if (presentIndex != -1 && presentQueue != VK_NULL_HANDLE)
		printf("Acquired Presentation Queue Handle: Index = %d\n", presentIndex);
	free(queueList);
}

VkSurfaceFormatKHR chooseSurfaceFormat(VkSurfaceFormatKHR* surfaceFormats, uint32_t formatCount)
{
	int bgr = 0, srgb = 0;
	for (uint32_t formatIndex = 0; formatIndex < formatCount; formatIndex++)
	{
		if (surfaceFormats[formatIndex].format == VK_FORMAT_UNDEFINED ||
		  surfaceFormats[formatIndex].format == VK_FORMAT_B8G8R8A8_UNORM)
			bgr = 1;
		if (surfaceFormats[formatIndex].format == VK_FORMAT_UNDEFINED ||
		  surfaceFormats[formatIndex].format == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			srgb = 1;
	}

	VkSurfaceFormatKHR temporaryFormat = { 0 };
	temporaryFormat.format = bgr ? VK_FORMAT_B8G8R8A8_UNORM : surfaceFormats[0].format;
	temporaryFormat.colorSpace = srgb ? VK_COLOR_SPACE_SRGB_NONLINEAR_KHR : surfaceFormats[0].colorSpace;
	return temporaryFormat;
}

VkPresentModeKHR choosePresentationMode(VkPresentModeKHR* presentModes, uint32_t modeCount)
{
	int mailbox = 0, relaxed = 0;
	for (uint32_t modeIndex = 0; modeIndex < modeCount; modeIndex++)
	{
		if (presentModes[modeIndex] == VK_PRESENT_MODE_MAILBOX_KHR)
			mailbox = 1;
		else if (presentModes[modeIndex] == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
			relaxed = 1;
	}

	if (mailbox)
		return VK_PRESENT_MODE_MAILBOX_KHR;
	else if (relaxed)
		return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	else
		return VK_PRESENT_MODE_FIFO_KHR;
}

void createSwapchain()
{
	VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(swapchainDetails.surfaceFormats, swapchainDetails.formatCount);
	VkPresentModeKHR presentMode = choosePresentationMode(swapchainDetails.presentModes, swapchainDetails.modeCount);
	framebufferSize = swapchainDetails.capabilities.minImageCount +
		(swapchainDetails.capabilities.minImageCount < swapchainDetails.capabilities.maxImageCount);
	swapchainFormat = surfaceFormat.format;
	swapchainExtent = swapchainDetails.capabilities.currentExtent;
	
	VkSwapchainCreateInfoKHR swapchainInfo = {0};
	swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainInfo.surface = surface;
	swapchainInfo.oldSwapchain = VK_NULL_HANDLE;
	swapchainInfo.minImageCount = framebufferSize;
	swapchainInfo.imageFormat = swapchainFormat;
	swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapchainInfo.presentMode = presentMode;
	swapchainInfo.clipped = VK_TRUE;
	swapchainInfo.imageExtent = swapchainExtent;
	swapchainInfo.imageArrayLayers = 1;
	swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainInfo.preTransform = swapchainDetails.capabilities.currentTransform;
	swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (graphicsIndex != presentIndex)
	{
		uint32_t queueIndices[] = { graphicsIndex, presentIndex };
		swapchainInfo.queueFamilyIndexCount = 2;
		swapchainInfo.pQueueFamilyIndices = queueIndices;
		swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
	}

	if (vkCreateSwapchainKHR(device, &swapchainInfo, NULL, &swapchain) == VK_SUCCESS)
		printf("Created Swapchain: %s\n", presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "Mailbox" : "FIFO");
	vkGetSwapchainImagesKHR(device, swapchain, &framebufferSize, NULL);
	swapchainImages = malloc(framebufferSize * sizeof(VkImage));
	vkGetSwapchainImagesKHR(device, swapchain, &framebufferSize, swapchainImages);
	if (framebufferSize && swapchainImages)
		printf("Acquired Swapchain Images: Count = %d\n", framebufferSize);
}

void createImageViews()
{
	swapchainViews = malloc(framebufferSize * sizeof(VkImageView));
	for (uint32_t viewIndex = 0; viewIndex < framebufferSize; viewIndex++)
	{
		VkImageViewCreateInfo viewInfo = {0};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = swapchainImages[viewIndex];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = swapchainFormat;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		if (vkCreateImageView(device, &viewInfo, NULL, &swapchainViews[viewIndex]) == VK_SUCCESS)
			printf("Acquired Image View: Number = %d\n", viewIndex);
	}
}

void createRenderPass()
{
	VkAttachmentReference colorAttachmentReference = { 0 };
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = { 0 };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;

	VkAttachmentDescription colorAttachment = { 0 };
	colorAttachment.format = swapchainFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkSubpassDependency dependency = { 0 };
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo = { 0 };
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device, &renderPassInfo, NULL, &renderPass) == VK_SUCCESS)
		printf("Created Render Pass: Subpass Count = %d\n", renderPassInfo.subpassCount);
}

VkShaderModule initializeShaderModule(const char* shaderName, const char* filePath)
{
	FILE *file = fopen(filePath, "rb");
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	rewind(file);
	size += size % 4 ? 4 - size % 4 : 0;
	
	uint32_t *shaderData = calloc(size, 1);
	fread(shaderData, size, 1, file);
	fclose(file);

	VkShaderModuleCreateInfo shaderInfo = { 0 };
	shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderInfo.codeSize = size;
	shaderInfo.pCode = shaderData;
	
	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device, &shaderInfo, NULL, &shaderModule) == VK_SUCCESS)
		printf("Created %s Shader Module: %zd bytes\n", shaderName, size);
	free(shaderData);
	return shaderModule;
}

void createGraphicsPipeline()
{
	VkShaderModule vertexShader = initializeShaderModule("Vertex", "shaders/vert.spv");
	VkPipelineShaderStageCreateInfo vertexStageInfo = { 0 };
	vertexStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexStageInfo.module = vertexShader;
	vertexStageInfo.pName = "main";

	VkShaderModule fragmentShader = initializeShaderModule("Fragment", "shaders/frag.spv");
	VkPipelineShaderStageCreateInfo fragmentStageInfo = { 0 };
	fragmentStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentStageInfo.module = fragmentShader;
	fragmentStageInfo.pName = "main";

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = { 0 };
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = { 0 };
	inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = { 0 };
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapchainExtent.width;
	viewport.height = (float)swapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkOffset2D offset = { 0, 0 };
	VkRect2D scissor = { 0 };
	scissor.offset = offset;
	scissor.extent = swapchainExtent;

	VkPipelineViewportStateCreateInfo viewportStateInfo = { 0 };
	viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.pViewports = &viewport;
	viewportStateInfo.scissorCount = 1;
	viewportStateInfo.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizerInfo = { 0 };
	rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerInfo.lineWidth = 1.0f;
	rasterizerInfo.depthClampEnable = VK_FALSE;
	rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerInfo.cullMode = VK_CULL_MODE_NONE;
	rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizerInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisamplingInfo = { 0 };
	multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisamplingInfo.sampleShadingEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = { 0 };
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendInfo = { 0 };
	colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendInfo.logicOpEnable = VK_FALSE;
	colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendInfo.attachmentCount = 1;
	colorBlendInfo.pAttachments = &colorBlendAttachment;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &pipelineLayout) == VK_SUCCESS)
		printf("Created Pipeline Layout: Empty\n");
	
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexStageInfo, fragmentStageInfo };
	VkGraphicsPipelineCreateInfo pipelineInfo = { 0 };
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
	pipelineInfo.pViewportState = &viewportStateInfo;
	pipelineInfo.pRasterizationState = &rasterizerInfo;
	pipelineInfo.pMultisampleState = &multisamplingInfo;
	pipelineInfo.pColorBlendState = &colorBlendInfo;
	pipelineInfo.layout = pipelineLayout;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &graphicsPipeline) == VK_SUCCESS)
		printf("Created Graphics Pipeline: %d x %d\n", width, height);
	
	vkDestroyShaderModule(device, fragmentShader, NULL);
	vkDestroyShaderModule(device, vertexShader, NULL);
}

void createFramebuffers()
{
	swapchainFramebuffers = malloc(framebufferSize * sizeof(VkFramebuffer));
	for (uint32_t framebufferIndex = 0; framebufferIndex < framebufferSize; framebufferIndex++)
	{
		VkFramebufferCreateInfo framebufferInfo = { 0 };
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &swapchainViews[framebufferIndex];
		framebufferInfo.width = swapchainExtent.width;
		framebufferInfo.height = swapchainExtent.height;
		framebufferInfo.layers = 1;
		if (vkCreateFramebuffer(device, &framebufferInfo, NULL, &swapchainFramebuffers[framebufferIndex]) == VK_SUCCESS)
			printf("Created Framebuffer: Number = %u\n", framebufferIndex);
	}
}

void createCommandPool()
{
	VkCommandPoolCreateInfo poolInfo = { 0 };
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = graphicsIndex;
	if (vkCreateCommandPool(device, &poolInfo, NULL, &commandPool) == VK_SUCCESS)
		printf("Created Command Pool: Queue Index = %d\n", graphicsIndex);
}

void createCommandBuffers()
{
	commandBuffers = malloc(framebufferSize * sizeof(VkCommandBuffer));
	VkCommandBufferAllocateInfo allocateInfo = { 0 };
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = framebufferSize;
	if (vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers) == VK_SUCCESS)
		printf("Allocated Command Buffers: Size = %d\n", framebufferSize);

	for (uint32_t commandIndex = 0; commandIndex < framebufferSize; commandIndex++)
	{
		VkCommandBufferBeginInfo beginInfo = { 0 };
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		if (vkBeginCommandBuffer(commandBuffers[commandIndex], &beginInfo) == VK_SUCCESS)
			printf("Started Command Buffer Recording: Index = %u\n", commandIndex);

		VkOffset2D offset = { 0, 0 };
		VkClearValue backgroundColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
		VkRenderPassBeginInfo renderPassBeginInfo = { 0 };
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = swapchainFramebuffers[commandIndex];
		renderPassBeginInfo.renderArea.offset = offset;
		renderPassBeginInfo.renderArea.extent = swapchainExtent;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &backgroundColor;
		vkCmdBeginRenderPass(commandBuffers[commandIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffers[commandIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
		vkCmdDraw(commandBuffers[commandIndex], 3, 1, 0, 0);
		vkCmdEndRenderPass(commandBuffers[commandIndex]);
		if (vkEndCommandBuffer(commandBuffers[commandIndex]) == VK_SUCCESS)
			printf("Successfuly Completed Recording: Index = %u\n", commandIndex);
	}
}

void createSyncObjects()
{
	framebufferLimit = 2;
	imageAvailable = malloc(framebufferLimit * sizeof(VkSemaphore));
	renderFinished = malloc(framebufferLimit * sizeof(VkSemaphore));
	frameFences = malloc(framebufferLimit * sizeof(VkFence));

	VkSemaphoreCreateInfo semaphoreInfo = { 0 };
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo = { 0 };
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (uint32_t syncIndex = 0; syncIndex < framebufferLimit; syncIndex++)
	{
		vkCreateSemaphore(device, &semaphoreInfo, NULL, &imageAvailable[syncIndex]);
		vkCreateSemaphore(device, &semaphoreInfo, NULL, &renderFinished[syncIndex]);
		vkCreateFence(device, &fenceInfo, NULL, &frameFences[syncIndex]);
	}

	printf("Created Syncronization Objects: Count = %u\n", framebufferLimit);
}

void setup()
{
	createInstance();
	#ifndef NDEBUG
		registerMessenger();
	#endif
	createWindow();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapchain();
	createImageViews();
	createRenderPass();
	createGraphicsPipeline();
	createFramebuffers();
	createCommandPool();
	createCommandBuffers();
	createSyncObjects();
}

void cleanupSwapChain()
{
	for (uint32_t framebufferIndex = 0; framebufferIndex < framebufferSize; framebufferIndex++)
		vkDestroyFramebuffer(device, swapchainFramebuffers[framebufferIndex], NULL);
	vkFreeCommandBuffers(device, commandPool, framebufferSize, commandBuffers);
	vkDestroyPipeline(device, graphicsPipeline, NULL);
	vkDestroyPipelineLayout(device, pipelineLayout, NULL);
	vkDestroyRenderPass(device, renderPass, NULL);
	for (uint32_t viewIndex = 0; viewIndex < framebufferSize; viewIndex++)
		vkDestroyImageView(device, swapchainViews[viewIndex], NULL);
	vkDestroySwapchainKHR(device, swapchain, NULL);
	free(swapchainDetails.presentModes);
	free(swapchainDetails.surfaceFormats);
}

void recreateSwapchain()
{
	vkDeviceWaitIdle(device);
	cleanupSwapChain();
	swapchainDetails = generateSwapchainDetails(physicalDevice);

	createSwapchain();
	createImageViews();
	createRenderPass();
	createGraphicsPipeline();
	createFramebuffers();
	createCommandBuffers();
}

void clean()
{
	for (uint32_t syncIndex = 0; syncIndex < framebufferLimit; syncIndex++)
	{
		vkDestroyFence(device, frameFences[syncIndex], NULL);
		vkDestroySemaphore(device, renderFinished[syncIndex], NULL);
		vkDestroySemaphore(device, imageAvailable[syncIndex], NULL);
	}
	vkDestroyCommandPool(device, commandPool, NULL);
	for (uint32_t framebufferIndex = 0; framebufferIndex < framebufferSize; framebufferIndex++)
		vkDestroyFramebuffer(device, swapchainFramebuffers[framebufferIndex], NULL);
	vkDestroyPipeline(device, graphicsPipeline, NULL);
	vkDestroyPipelineLayout(device, pipelineLayout, NULL);
	vkDestroyRenderPass(device, renderPass, NULL);
	for (uint32_t viewIndex = 0; viewIndex < framebufferSize; viewIndex++)
		vkDestroyImageView(device, swapchainViews[viewIndex], NULL);
	vkDestroySwapchainKHR(device, swapchain, NULL);
	vkDestroyDevice(device, NULL);
	vkDestroySurfaceKHR(instance, surface, NULL);
	#ifdef __linux__
		xcb_destroy_window(xconn, window);
		xcb_disconnect(xconn);
	#endif
	#ifndef NDEBUG
		PFN_vkDestroyDebugUtilsMessengerEXT destroyMessenger =
			(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (destroyMessenger != NULL)
			destroyMessenger(instance, messenger, NULL);
	#endif
	vkDestroyInstance(instance, NULL);
}

#ifdef _WIN32
	LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (message == WM_DESTROY)
		{
			PostQuitMessage(0);
			return 0;
		}
		else if (message == WM_SIZING)
		{
			RECT rect;
			GetClientRect(hWnd, &rect);
			width = rect.right;
			height = rect.bottom;
			recreateSwapchain();
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
#endif

void draw()
{
	int currentFrame = 0, frameCount = 0;

	while (1)
	{
		#ifdef _WIN32
			MSG msg;
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				if (msg.message == WM_QUIT)
					break;
			}
		#elif __linux__
			event = xcb_poll_for_event(xconn);
			if (event)
			{
				if ((event->response_type & ~0x80) == XCB_RESIZE_REQUEST)
				{
					width = ((xcb_resize_request_event_t*)event)->width;
					height = ((xcb_resize_request_event_t*)event)->height;
					recreateSwapchain();
					continue;
				}
				else if ((event->response_type & ~0x80) == XCB_CLIENT_MESSAGE &&
					((xcb_client_message_event_t*)event)->data.data32[0] == atom)
					break;
				free(event);
			}
		#endif

		vkWaitForFences(device, 1, &frameFences[currentFrame], VK_TRUE, ULONG_MAX);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(device, swapchain, ULONG_MAX,
			imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);
		if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapchain();
			continue;
		}
		
		VkSemaphore waitSemaphores[] = { imageAvailable[currentFrame] };
		VkSemaphore signalSemaphores[] = { renderFinished[currentFrame] };
		VkPipelineStageFlags stageFlags[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSubmitInfo submitInfo = { 0 };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = stageFlags;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkResetFences(device, 1, &frameFences[currentFrame]);
		vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameFences[currentFrame]);

		VkSwapchainKHR swapchains[] = { swapchain };
		VkPresentInfoKHR presentInfo = { 0 };
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &imageIndex;
		vkQueuePresentKHR(presentQueue, &presentInfo);

		frameCount++;
		currentFrame = frameCount % framebufferLimit;
	}

	vkDeviceWaitIdle(device);
}

int main()
{
	setup();
	draw();
	clean();
}
