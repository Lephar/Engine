#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <blis/blis.h>
#include <xcb/xcb.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>

struct swapchainDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	uint32_t formatCount, modeCount;
	VkSurfaceFormatKHR* surfaceFormats;
	VkPresentModeKHR* presentModes;
} typedef SwapchainDetails;

int width, height;
xcb_connection_t *xconn;
xcb_window_t window;
xcb_intern_atom_reply_t *reply;
xcb_generic_event_t *event;
const int maxFrames = 2;
VkInstance instance;
VkDebugUtilsMessengerEXT messenger;
uint32_t presentIndex, graphicsIndex;
VkPhysicalDevice physicalDevice;
VkQueue presentQueue, graphicsQueue;
VkDevice device;
VkSurfaceKHR surface;
SwapchainDetails details;
VkSwapchainKHR swapchain;
uint32_t swapchainImageCount;
VkImage *swapchainImages;
VkFormat swapchainImageFormat;
VkExtent2D swapchainExtent;
VkImageView *swapchainImageViews;
VkRenderPass renderPass;
VkPipelineLayout pipelineLayout;
VkPipeline graphicsPipeline;
VkFramebuffer *swapchainFramebuffers;
VkCommandPool commandPool;
VkCommandBuffer *commandBuffers;
VkSemaphore *imageAvailable, *renderFinished;
VkFence *frameFences;

static VKAPI_ATTR VkBool32 VKAPI_CALL messageCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		printf("%s\n", pCallbackData->pMessage);
	return VK_FALSE;
}

/*void blis()
{
	bli_thread_set_num_threads(6);

	dim_t m = 3840, n = 2160, k = 3840;
	inc_t rsa = 1, csa = m, rsb = 1, csb = k, rsc = 1, csc = m;
	double *A, *B, *C, alpha = 1, beta = 0;

	A = malloc(m * k * sizeof(double));
	B = malloc(k * n * sizeof(double));
	C = malloc(m * n * sizeof(double));

	bli_drandm(0, BLIS_DENSE, m, k, A, rsa, csa);
	bli_drandm(0, BLIS_DENSE, k, n, B, rsb, csb);
	bli_dsetm(BLIS_NO_CONJUGATE, 0, BLIS_NONUNIT_DIAG, BLIS_DENSE, m, n, &(double){0}, C, rsc, csc);

	//bli_dprintm("a:", m, k, a, rsa, csa, "%.4f", "");
	//bli_dprintm("b:", k, n, b, rsb, csb, "%.4f", "");

	//printf("Threads: %ld\n", bli_thread_get_num_threads());
	bli_dgemm(BLIS_NO_TRANSPOSE, BLIS_NO_TRANSPOSE, m, n, k, &alpha, A, rsa, csa, B, rsb, csb, &beta, C, rsc, csc);

	//bli_dprintm( "C:", m, n, C, rsc, csc, "%.4f", "");

	free(A);
	free(B);
	free(C);
}*/

void createWindow()
{
	width = 320;
	height = 240;
	xconn = xcb_connect(NULL, NULL);
	xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(xconn)).data;
	window = xcb_generate_id(xconn);
	//xcb_create_window(xconn, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, width, height, 0, 
	//	XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, 0, NULL);
	xcb_create_window(xconn, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, width, height, 0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, XCB_CW_EVENT_MASK,
		&(uint32_t){XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS});
	xcb_intern_atom_cookie_t oldCookie = xcb_intern_atom(xconn, 1, 12, "WM_PROTOCOLS");
	xcb_intern_atom_reply_t* oldReply = xcb_intern_atom_reply(xconn, oldCookie, 0);
	xcb_intern_atom_cookie_t newCookie = xcb_intern_atom(xconn, 0, 16, "WM_DELETE_WINDOW");
	reply = xcb_intern_atom_reply(xconn, newCookie, 0);
	xcb_change_property(xconn, XCB_PROP_MODE_REPLACE, window, oldReply->atom, 4, 32, 1, &reply->atom);
	xcb_change_property(xconn, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 7, "Vulkan");
	xcb_map_window(xconn, window);
	xcb_flush(xconn);
	printf("Created Window! (Xorg)\n");
}

void createInstance()
{
	VkApplicationInfo appInfo = { 0 };
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Tutorial";
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo instanceInfo = { 0 };
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.enabledExtensionCount = 3;
	instanceInfo.ppEnabledExtensionNames = (const char*[]) {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
			VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME
	};
	instanceInfo.enabledLayerCount = 1;
	instanceInfo.ppEnabledLayerNames = (const char*[]) { "VK_LAYER_LUNARG_standard_validation" };

	if (vkCreateInstance(&instanceInfo, NULL, &instance) == VK_SUCCESS)
		printf("Created Instance! (LunarG)\n");
}

void registerMessenger()
{
	VkDebugUtilsMessengerCreateInfoEXT messengerInfo = { 0 };
	messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	messengerInfo.messageSeverity = //VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
	//	VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	messengerInfo.pfnUserCallback = messageCallback;
	messengerInfo.pUserData = NULL;

	PFN_vkCreateDebugUtilsMessengerEXT createMessenger =
		(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (createMessenger != NULL && createMessenger(instance, &messengerInfo, NULL, &messenger) == VK_SUCCESS)
		printf("Registered Messenger! (Validation Layers)\n");
}

void createSurface()
{
	VkXcbSurfaceCreateInfoKHR surfaceInfo = { 0 };
	surfaceInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	surfaceInfo.connection = xconn;
	surfaceInfo.window = window;
	if (vkCreateXcbSurfaceKHR(instance, &surfaceInfo, NULL, &surface) == VK_SUCCESS)
		printf("Created Surface! (XCB Surface)\n");
}

void pickPhysicalDevice() //TODO: Generalize the picking
{
	physicalDevice = VK_NULL_HANDLE;

	uint32_t devc;
	vkEnumeratePhysicalDevices(instance, &devc, NULL);
	VkPhysicalDevice devs[devc];
	vkEnumeratePhysicalDevices(instance, &devc, devs);
	for (uint32_t devi = 0; devi < devc; devi++)
	{
		VkPhysicalDeviceProperties devp;
		vkGetPhysicalDeviceProperties(devs[devi], &devp);
		VkPhysicalDeviceFeatures devf;
		vkGetPhysicalDeviceFeatures(devs[devi], &devf);
		uint32_t extc, extv = 0;
		vkEnumerateDeviceExtensionProperties(devs[devi], NULL, &extc, NULL);
		VkExtensionProperties extp[extc];
		vkEnumerateDeviceExtensionProperties(devs[devi], NULL, &extc, extp);
		for (uint32_t exti = 0; exti < extc; exti++)
			if (!strcmp(extp[exti].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
				extv = 1;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devs[devi], surface, &details.capabilities);
		vkGetPhysicalDeviceSurfaceFormatsKHR(devs[devi], surface, &details.formatCount, NULL);
		vkGetPhysicalDeviceSurfacePresentModesKHR(devs[devi], surface, &details.modeCount, NULL);
		details.surfaceFormats = malloc(details.formatCount * sizeof(VkSurfaceFormatKHR));
		details.presentModes = malloc(details.modeCount * sizeof(VkPresentModeKHR));
		vkGetPhysicalDeviceSurfaceFormatsKHR(devs[devi], surface, &details.formatCount, details.surfaceFormats);
		vkGetPhysicalDeviceSurfacePresentModesKHR(devs[devi], surface, &details.modeCount, details.presentModes);

		if (extv && details.formatCount && details.modeCount && devf.geometryShader
			&& devp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			physicalDevice = devs[devi];
			printf("Picked Physical Device! (Index: %d)\n", devi);
			break;
		}
		else
		{
			details.capabilities = (VkSurfaceCapabilitiesKHR) { 0 };
			details.formatCount = 0;
			details.modeCount = 0;
			free(details.surfaceFormats);
			free(details.presentModes);
		}
	}
}

void createLogicalDevice()
{
	graphicsIndex = presentIndex = -1;
	graphicsQueue = presentQueue = VK_NULL_HANDLE;

	uint32_t quec;
	float queuePriority = 1.0f;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &quec, NULL);
	VkQueueFamilyProperties quep[quec];
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &quec, quep);
	for (uint32_t quei = 0; quei < quec; quei++)
	{
		VkBool32 presentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, quei, surface, &presentSupport);
		if (quep[quei].queueCount > 0 && presentSupport)
			presentIndex = quei;
		if (quep[quei].queueCount > 0 && quep[quei].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			graphicsIndex = quei;
	}
	VkDeviceQueueCreateInfo graphicsInfo = { 0 };
	graphicsInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	graphicsInfo.queueCount = 1;
	graphicsInfo.queueFamilyIndex = graphicsIndex;
	graphicsInfo.pQueuePriorities = &queuePriority;
	VkDeviceQueueCreateInfo presentInfo = { 0 };
	presentInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	presentInfo.queueCount = 1;
	presentInfo.queueFamilyIndex = presentIndex;
	presentInfo.pQueuePriorities = &queuePriority;

	VkDeviceCreateInfo deviceInfo = { 0 };
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	VkPhysicalDeviceFeatures devf = { 0 };
	deviceInfo.pEnabledFeatures = &devf;
	deviceInfo.enabledLayerCount = 1;
	deviceInfo.ppEnabledLayerNames = (const char*[]) { "VK_LAYER_LUNARG_standard_validation" };
	deviceInfo.enabledExtensionCount = 1;
	deviceInfo.ppEnabledExtensionNames = (const char*[]) { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	deviceInfo.queueCreateInfoCount = 1;
	if (graphicsIndex != presentIndex)
		deviceInfo.queueCreateInfoCount++;
	deviceInfo.pQueueCreateInfos = (VkDeviceQueueCreateInfo[]) { graphicsInfo, presentInfo };

	if (vkCreateDevice(physicalDevice, &deviceInfo, NULL, &device) == VK_SUCCESS)
		printf("Created Logical Device! (Queue Count: %d)\n", deviceInfo.queueCreateInfoCount);
	vkGetDeviceQueue(device, graphicsIndex, 0, &graphicsQueue);
	if (graphicsIndex != -1 && graphicsQueue != VK_NULL_HANDLE)
		printf("Acquired Graphics Queue Handle! (Index: %d)\n", graphicsIndex);
	vkGetDeviceQueue(device, presentIndex, 0, &presentQueue);
	if (presentIndex != -1 && presentQueue != VK_NULL_HANDLE)
		printf("Acquired Presentation Queue Handle! (Index: %d)\n", presentIndex);
}

void createSwapchain() //TODO: Check support before setting formats
{
	swapchainImageCount = 0;
	swapchainImages = NULL;
	swapchainImageFormat = details.surfaceFormats[0].format;
	swapchainExtent = details.capabilities.currentExtent;

	VkSwapchainCreateInfoKHR swapchainInfo = { 0 };
	swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainInfo.surface = surface;
	swapchainInfo.minImageCount = details.capabilities.minImageCount < details.capabilities.maxImageCount
		? details.capabilities.minImageCount + 1 : details.capabilities.maxImageCount;
	swapchainInfo.imageFormat = swapchainImageFormat;
	swapchainInfo.imageColorSpace = details.surfaceFormats[0].colorSpace;
	swapchainInfo.presentMode = details.presentModes[0];
	swapchainInfo.clipped = VK_TRUE;
	swapchainInfo.imageExtent = swapchainExtent;
	swapchainInfo.imageArrayLayers = 1;
	swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainInfo.preTransform = details.capabilities.currentTransform;
	swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainInfo.oldSwapchain = VK_NULL_HANDLE;
	if (graphicsQueue == presentQueue)
		swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	else
	{
		swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchainInfo.queueFamilyIndexCount = 2;
		swapchainInfo.pQueueFamilyIndices = (uint32_t[]) { presentIndex, graphicsIndex };
	}

	if (vkCreateSwapchainKHR(device, &swapchainInfo, NULL, &swapchain) == VK_SUCCESS)
		printf("Created Swapchain! (BGRA SRGB FIFO)\n");
	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, NULL);
	swapchainImages = malloc(swapchainImageCount * sizeof(VkImage));
	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages);
	if (swapchainImageCount && swapchainImages)
		printf("Acquired Swapchain Images! (Count: %d)\n", swapchainImageCount);
}

void createImageViews()
{
	swapchainImageViews = malloc(swapchainImageCount * sizeof(VkImageView));
	for (uint32_t i = 0; i < swapchainImageCount; i++)
	{
		VkImageViewCreateInfo viewInfo = { 0 };
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = swapchainImages[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = swapchainImageFormat;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		if (vkCreateImageView(device, &viewInfo, NULL, &swapchainImageViews[i]) == VK_SUCCESS)
			printf("Acquired Image View! (Number: %d)\n", i);
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
	colorAttachment.format = swapchainImageFormat;
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
		printf("Created Render Pass! (Subpass Count: %d)\n", renderPassInfo.subpassCount);
}

void createGraphicsPipeline() //TODO: so much to do...
{
	size_t size;
	FILE *file;
	VkShaderModule vertexShaderModule, fragmentShaderModule;
	//byte values in int bodies, just spir-v things

	file = fopen("shaders/vert.spv", "r");
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	rewind(file);
	size += size % 4 ? 4 - size % 4 : 0;
	uint32_t *vertexShader = calloc(size, 1);
	fread(vertexShader, size, 1, file);
	fclose(file);
	VkShaderModuleCreateInfo vertexInfo = { 0 };
	vertexInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexInfo.codeSize = size;
	vertexInfo.pCode = vertexShader;
	if (vkCreateShaderModule(device, &vertexInfo, NULL, &vertexShaderModule) == VK_SUCCESS)
		printf("Created Vertex Shader Module! (%ld bytes)\n", size);

	file = fopen("shaders/frag.spv", "r");
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	rewind(file);
	size += size % 4 ? 4 - size % 4 : 0;
	uint32_t *fragmentShader = calloc(size, 1);
	fread(fragmentShader, size, 1, file);
	fclose(file);
	VkShaderModuleCreateInfo fragmentInfo = { 0 };
	fragmentInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentInfo.codeSize = size;
	fragmentInfo.pCode = fragmentShader;
	if (vkCreateShaderModule(device, &fragmentInfo, NULL, &fragmentShaderModule) == VK_SUCCESS)
		printf("Created Fragment Shader Module! (%ld bytes)\n", size);

	VkPipelineShaderStageCreateInfo vertexStageInfo = { 0 };
	vertexStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexStageInfo.module = vertexShaderModule;
	vertexStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragmentStageInfo = { 0 };
	fragmentStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentStageInfo.module = fragmentShaderModule;
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

	VkRect2D scissor = { 0 };
	scissor.offset = (VkOffset2D) { 0, 0 };
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
	//rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizerInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisamplingInfo = { 0 };
	multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisamplingInfo.sampleShadingEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
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
	//colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlendInfo.attachmentCount = 1;
	colorBlendInfo.pAttachments = &colorBlendAttachment;

	/*VkPipelineDynamicStateCreateInfo dynamicStateInfo = {0};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = 1;
	dynamicStateInfo.pDynamicStates = (VkDynamicState[]){VK_DYNAMIC_STATE_VIEWPORT};*/

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &pipelineLayout) == VK_SUCCESS)
		printf("Created Pipeline Layout! (Empty Yet)\n");

	VkGraphicsPipelineCreateInfo pipelineInfo = { 0 };
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = (VkPipelineShaderStageCreateInfo[]) { vertexStageInfo, fragmentStageInfo };
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
	pipelineInfo.pViewportState = &viewportStateInfo;
	pipelineInfo.pRasterizationState = &rasterizerInfo;
	pipelineInfo.pMultisampleState = &multisamplingInfo;
	//pipelineInfo.pDepthStencilState = NULL;
	pipelineInfo.pColorBlendState = &colorBlendInfo;
	//pipelineInfo.pDynamicState = &dynamicStateInfo;
	pipelineInfo.layout = pipelineLayout;
	//pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	//pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &graphicsPipeline) == VK_SUCCESS)
		printf("Created Graphics Pipeline! (Viewport: %d %d)\n", width, height);

	vkDestroyShaderModule(device, fragmentShaderModule, NULL);
	vkDestroyShaderModule(device, vertexShaderModule, NULL);
}

void createFramebuffers()
{
	swapchainFramebuffers = malloc(swapchainImageCount * sizeof(VkFramebuffer));
	for (size_t i = 0; i < swapchainImageCount; i++)
	{
		VkFramebufferCreateInfo framebufferInfo = { 0 };
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &swapchainImageViews[i];
		framebufferInfo.width = swapchainExtent.width;
		framebufferInfo.height = swapchainExtent.height;
		framebufferInfo.layers = 1;
		if (vkCreateFramebuffer(device, &framebufferInfo, NULL, &swapchainFramebuffers[i]) == VK_SUCCESS)
			printf("Created Framebuffer! (Number: %ld)\n", i);
	}
}

void createCommandBuffers() //TODO: read the manual again
{
	VkCommandPoolCreateInfo poolInfo = { 0 };
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = graphicsIndex;
	if (vkCreateCommandPool(device, &poolInfo, NULL, &commandPool) == VK_SUCCESS)
		printf("Created Command Pool! (Queue Index: %d)\n", graphicsIndex);

	commandBuffers = malloc(swapchainImageCount * sizeof(VkCommandBuffer));
	VkCommandBufferAllocateInfo allocateInfo = { 0 };
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = swapchainImageCount;
	if (vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers) == VK_SUCCESS)
		printf("Created Command Buffers! (Size: %d)\n", swapchainImageCount);

	for (size_t i = 0; i < swapchainImageCount; i++)
	{
		VkCommandBufferBeginInfo beginInfo = { 0 };
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) == VK_SUCCESS)
			printf("Started Command Buffer Recording! (Index: %ld)\n", i);

		VkRenderPassBeginInfo renderPassBeginInfo = { 0 };
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = swapchainFramebuffers[i];
		renderPassBeginInfo.renderArea.offset = (VkOffset2D) { 0, 0 };
		renderPassBeginInfo.renderArea.extent = swapchainExtent;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &(VkClearValue) { { {0.0f, 0.0f, 0.0f, 1.0f}} };
		vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		printf("Started Render Pass On Command Buffer! (Index: %ld)\n", i);
		vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
		printf("Bound Graphics Pipeline To Command Buffer! (Index: %ld)\n", i);
		vkCmdDraw(commandBuffers[i], 3, 1, 0, 0); //?
		printf("Sending Draw Commands! (Index: %ld)\n", i);
		vkCmdEndRenderPass(commandBuffers[i]); //???
		if (vkEndCommandBuffer(commandBuffers[i]) == VK_SUCCESS) //?????
			printf("Successfuly Completed Recording! (Index: %ld)\n", i);
	}
}

void createSyncObjects()
{
	imageAvailable = malloc(maxFrames * sizeof(VkSemaphore));
	renderFinished = malloc(maxFrames * sizeof(VkSemaphore));
	frameFences = malloc(maxFrames * sizeof(VkFence));

	VkSemaphoreCreateInfo semaphoreInfo = { 0 };
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int i = 0; i < maxFrames; i++)
	{
		vkCreateSemaphore(device, &semaphoreInfo, NULL, &imageAvailable[i]);
		vkCreateSemaphore(device, &semaphoreInfo, NULL, &renderFinished[i]);
		vkCreateFence(device, &fenceInfo, NULL, &frameFences[i]);
	}

	printf("Created Syncronization Objects! (Count: %d)\n", maxFrames);
}

void setup()
{
	createWindow();
	createInstance();
	registerMessenger();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapchain();
	createImageViews();
	createRenderPass();
	createGraphicsPipeline();
	createFramebuffers();
	createCommandBuffers();
	createSyncObjects();
}

void draw()
{
	int currentFrame = 0, frameCount = 0;

	while (1)
	{
		event = xcb_poll_for_event(xconn);
		if (event && (event->response_type & ~0x80) == XCB_CLIENT_MESSAGE &&
			((xcb_client_message_event_t*)event)->data.data32[0] == reply->atom)
			break;
		free(event);

		vkWaitForFences(device, 1, &frameFences[currentFrame], VK_TRUE, ULONG_MAX);
		vkResetFences(device, 1, &frameFences[currentFrame]);

		uint32_t imageIndex;
		vkAcquireNextImageKHR(device, swapchain, ULONG_MAX,
			imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

		VkSubmitInfo submitInfo = { 0 };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = (VkSemaphore[]) { imageAvailable[currentFrame] };
		submitInfo.pWaitDstStageMask = (VkPipelineStageFlags[]) { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = (VkSemaphore[]) { renderFinished[currentFrame] };
		vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameFences[currentFrame]);
		//if(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS)
		//	printf("Submitted Queue!\n");

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = (VkSemaphore[]) { renderFinished[currentFrame] };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = (VkSwapchainKHR[]) { swapchain };
		presentInfo.pImageIndices = &imageIndex;
		vkQueuePresentKHR(presentQueue, &presentInfo);
		//	printf("Presenting Queue!\n");
		//vkQueueWaitIdle(presentQueue);

		frameCount++;
		currentFrame = frameCount % maxFrames;
		//if(frameCount % 60 == 0)
		//	printf("Frame: %d!\n", frameCount);
	}

	vkDeviceWaitIdle(device);
}

void clean()
{
	printf("Started Cleanup!\n");
	for (int i = 0; i < maxFrames; i++)
	{
		vkDestroyFence(device, frameFences[i], NULL);
		vkDestroySemaphore(device, renderFinished[i], NULL);
		vkDestroySemaphore(device, imageAvailable[i], NULL);
	}
	vkDestroyCommandPool(device, commandPool, NULL);
	for (size_t i = 0; i < swapchainImageCount; i++)
		vkDestroyFramebuffer(device, swapchainFramebuffers[i], NULL);
	vkDestroyPipeline(device, graphicsPipeline, NULL);
	vkDestroyPipelineLayout(device, pipelineLayout, NULL);
	vkDestroyRenderPass(device, renderPass, NULL);
	for (uint32_t i = 0; i < swapchainImageCount; i++)
		vkDestroyImageView(device, swapchainImageViews[i], NULL);
	vkDestroySwapchainKHR(device, swapchain, NULL);
	vkDestroyDevice(device, NULL);
	vkDestroySurfaceKHR(instance, surface, NULL);
	PFN_vkDestroyDebugUtilsMessengerEXT destroyMessenger =
		(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (destroyMessenger != NULL)
		destroyMessenger(instance, messenger, NULL);
	vkDestroyInstance(instance, NULL);
	free(event);
	xcb_destroy_window(xconn, window);
	xcb_disconnect(xconn);
	printf("Finished Cleanup!\n");
}

int main()
{
	setup();
	draw();
	clean();

	return 0;
}