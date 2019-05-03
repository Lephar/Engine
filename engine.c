#include <time.h>
#include <math.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/mman.h>
#include <xcb/xcb.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>

#define STBI_ASSERT(x)
#define STB_IMAGE_IMPLEMENTATION
#include "libraries/stb_image.h"
#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "libraries/tinyobj_loader_c.h"

#define PI 3.14159265358979f
#define varname(variable) (#variable)

struct vertex
{
	float pos[3];
	float col[3];
	float tex[2];
};

struct uniformBufferObject
{
	float model[16];
	float view[16];
	float proj[16];
};

struct swapchainDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	uint32_t formatCount, modeCount;
	VkSurfaceFormatKHR *surfaceFormats;
	VkPresentModeKHR *presentModes;
};

typedef struct vertex Vertex;
typedef struct uniformBufferObject UniformBufferObject;
typedef struct swapchainDetails SwapchainDetails;

uint32_t width, height;
xcb_connection_t *xconn;
xcb_window_t window;
xcb_generic_event_t *event;
xcb_atom_t destroyEvent;
struct timespec timespec;

VkInstance instance;
VkDebugUtilsMessengerEXT messenger;
VkSurfaceKHR surface;
VkPhysicalDevice physicalDevice;
SwapchainDetails swapchainDetails;
VkDevice device;
uint32_t graphicsIndex, presentIndex;
VkQueue graphicsQueue, presentQueue;
uint32_t framebufferSize, framebufferLimit;
VkSwapchainKHR swapchain;
VkFormat swapchainFormat;
VkExtent2D swapchainExtent;
VkImage *swapchainImages;
VkImageView *swapchainViews;
VkRenderPass renderPass;
VkDescriptorSetLayout descriptorSetLayout;
VkPipelineLayout pipelineLayout;
VkPipeline graphicsPipeline;
VkFramebuffer *swapchainFramebuffers;
VkDeviceSize vertexCount, vertexSize, indexCount, indexSize;
Vertex *vertices;
uint32_t *indices;
VkBuffer vertexBuffer, indexBuffer;
VkBuffer *uniformBuffers;
VkDeviceMemory vertexBufferMemory, indexBufferMemory;
VkDeviceMemory *uniformBufferMemories;
VkCommandPool commandPool;
VkCommandBuffer *commandBuffers;
VkImage textureImage, depthImage;
VkImageView textureView, depthView;
VkDeviceMemory textureMemory, depthMemory;
VkSampler textureSampler;
VkDescriptorPool descriptorPool;
VkDescriptorSet *descriptorSets;
VkSemaphore *imageAvailable, *renderFinished;
VkFence *frameFences;

void setup();
void clean();
void recreateSwapchain();
void cleanupSwapchain();

void printlog(int success, const char *caller, char *format, ...)
{
	if(success)
	{
		va_list ap;
		va_start(ap, format);
		vprintf(format, ap);
		va_end(ap);
	}

	else
	{
		printf("Failed at %s()\n", caller);
		exit(1);
	}
}

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

	printlog(vkCreateInstance(&instanceInfo, NULL, &instance) == VK_SUCCESS,
	 __FUNCTION__, "Created Vulkan Instance\n");
}

static VKAPI_ATTR VkBool32 VKAPI_CALL messageCallback(
 VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
 const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
	(void)type;
	(void)severity;
	(void)pUserData;

	printlog(1, NULL, "%s\n", pCallbackData->pMessage);
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
	printlog(createMessenger != NULL && createMessenger(instance, &messengerInfo, NULL, &messenger) == VK_SUCCESS,
	 __FUNCTION__, "Registered Validation Layer Messenger\n");
}

void createWindow()
{
	width = 640;
	height = 640;
	xconn = xcb_connect(NULL, NULL);
	xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(xconn)).data;
	window = xcb_generate_id(xconn);

	uint32_t valueList[] = {screen->black_pixel,
	 XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_STRUCTURE_NOTIFY};
	xcb_create_window(xconn, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, width, height, 0,
	 XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, valueList);

	xcb_intern_atom_cookie_t protCookie = xcb_intern_atom(xconn, 0, 12, "WM_PROTOCOLS");
	xcb_intern_atom_reply_t* protReply = xcb_intern_atom_reply(xconn, protCookie, 0);
	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(xconn, 0, 16, "WM_DELETE_WINDOW");
	xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(xconn, cookie, 0);
	xcb_change_property(xconn, XCB_PROP_MODE_REPLACE, window, protReply->atom,
	 XCB_ATOM_ATOM, 32, 1, &reply->atom);
	destroyEvent = reply->atom;

	xcb_map_window(xconn, window);
	xcb_flush(xconn);
	printlog(1, NULL, "Created XCB Window\n");
}

void createSurface()
{
	VkXcbSurfaceCreateInfoKHR surfaceInfo = {0};
	surfaceInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	surfaceInfo.connection = xconn;
	surfaceInfo.window = window;

	printlog(vkCreateXcbSurfaceKHR(instance, &surfaceInfo, NULL, &surface) == VK_SUCCESS,
	 __FUNCTION__, "Created XCB Surface\n");
}

SwapchainDetails generateSwapchainDetails(VkPhysicalDevice temporaryDevice)
{
	SwapchainDetails temporaryDetails = {0};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(temporaryDevice, surface, &temporaryDetails.capabilities);
	vkGetPhysicalDeviceSurfaceFormatsKHR(temporaryDevice, surface, &temporaryDetails.formatCount, NULL);
	vkGetPhysicalDeviceSurfacePresentModesKHR(temporaryDevice, surface, &temporaryDetails.modeCount, NULL);
	temporaryDetails.surfaceFormats = malloc(temporaryDetails.formatCount * sizeof(VkSurfaceFormatKHR));
	temporaryDetails.presentModes = malloc(temporaryDetails.modeCount * sizeof(VkPresentModeKHR));
	vkGetPhysicalDeviceSurfaceFormatsKHR(temporaryDevice, surface,
	 &temporaryDetails.formatCount, temporaryDetails.surfaceFormats);
	vkGetPhysicalDeviceSurfacePresentModesKHR(temporaryDevice, surface,
	 &temporaryDetails.modeCount, temporaryDetails.presentModes);
	return temporaryDetails;
}

void pickPhysicalDevice()
{
	int32_t maxScore = -1, bestIndex = -1;
	uint32_t deviceCount;
	char *deviceName = malloc(VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);

	vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);
	VkPhysicalDevice *devices = malloc(deviceCount * sizeof(VkPhysicalDevice));
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices);

	for(uint32_t deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++)
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
		for(uint32_t extensionIndex = 0; extensionIndex < extensionCount; extensionIndex++)
			if((swapchainSupport =
			 !strcmp(extensionProperties[extensionIndex].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)))
				break;
		free(extensionProperties);

		if(formatCount && modeCount && swapchainSupport &&
		 deviceFeatures.geometryShader && deviceFeatures.samplerAnisotropy)
		{
			int32_t deviceScore = extensionCount + (formatCount + modeCount) * 16;
			if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				deviceScore *= 2;

			if(deviceScore >= maxScore)
			{
				maxScore = deviceScore;
				bestIndex = deviceIndex;
				strncpy(deviceName, deviceProperties.deviceName, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);
			}
		}
	}

	printlog(maxScore != -1, __FUNCTION__, "Picked Physical Device: %s\n", deviceName);
	physicalDevice = devices[bestIndex];
	swapchainDetails = generateSwapchainDetails(physicalDevice);
	free(deviceName);
	free(devices);
}

void createLogicalDevice()
{
	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, NULL);
	VkQueueFamilyProperties *queueProperties = malloc(queueCount * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProperties);

	for(uint32_t queueIndex = 0; queueIndex < queueCount; queueIndex++)
	{
		VkBool32 presentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueIndex, surface, &presentSupport);
		if(queueProperties[queueIndex].queueCount > 0 && presentSupport)
			presentIndex = queueIndex;
		if(queueProperties[queueIndex].queueCount > 0 &&
		 queueProperties[queueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			graphicsIndex = queueIndex;
	}

	float queuePriority = 1.0f;
	queueCount = graphicsIndex == presentIndex ? 1 : 2;
	VkDeviceQueueCreateInfo *queueList = malloc(queueCount * sizeof(VkDeviceQueueCreateInfo));

	VkDeviceQueueCreateInfo graphicsInfo = {0};
	graphicsInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	graphicsInfo.queueCount = 1;
	graphicsInfo.queueFamilyIndex = graphicsIndex;
	graphicsInfo.pQueuePriorities = &queuePriority;
	queueList[0] = graphicsInfo;

	if(queueCount == 2)
	{
		VkDeviceQueueCreateInfo presentInfo = {0};
		presentInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		presentInfo.queueCount = 1;
		presentInfo.queueFamilyIndex = presentIndex;
		presentInfo.pQueuePriorities = &queuePriority;
		queueList[1] = presentInfo;
	}

	VkPhysicalDeviceFeatures deviceFeatures = {0};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	const char *layerNames[] = {"VK_LAYER_LUNARG_standard_validation"};
	uint32_t layerCount = sizeof(layerNames) / sizeof(layerNames[0]);
	const char *extensionNames[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	uint32_t extensionCount = sizeof(extensionNames) / sizeof(extensionNames[0]);

	VkDeviceCreateInfo deviceInfo = {0};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.enabledLayerCount = layerCount;
	deviceInfo.ppEnabledLayerNames = layerNames;
	deviceInfo.enabledExtensionCount = extensionCount;
	deviceInfo.ppEnabledExtensionNames = extensionNames;
	deviceInfo.pEnabledFeatures = &deviceFeatures;
	deviceInfo.queueCreateInfoCount = queueCount;
	deviceInfo.pQueueCreateInfos = queueList;

	printlog(vkCreateDevice(physicalDevice, &deviceInfo, NULL, &device) == VK_SUCCESS, __FUNCTION__,
	 "Created Logical Device: %s Queues\n", queueCount == 1 ? "Common" : "Specialized");
	vkGetDeviceQueue(device, graphicsIndex, 0, &graphicsQueue);
	vkGetDeviceQueue(device, presentIndex, 0, &presentQueue);
	free(queueList);
}

VkFormat chooseSupportedFormat(VkFormat *candidates, uint32_t candidateCount,
 VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for(uint32_t formatIndex = 0; formatIndex < candidateCount; formatIndex++)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, candidates[formatIndex], &properties);

		if((tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features) ||
		 (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features))
			return candidates[formatIndex];
	}

	return -1;
}

VkFormat chooseDepthFormat()
{
	return chooseSupportedFormat((VkFormat[])
	 {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT},
	 3, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkSurfaceFormatKHR chooseSurfaceFormat(VkSurfaceFormatKHR *surfaceFormats, uint32_t formatCount)
{
	int8_t bgr = 0, srgb = 0;

	for(uint32_t formatIndex = 0; formatIndex < formatCount; formatIndex++)
	{
		if(surfaceFormats[formatIndex].format == VK_FORMAT_UNDEFINED ||
		 surfaceFormats[formatIndex].format == VK_FORMAT_B8G8R8A8_UNORM)
			bgr = 1;

		if(surfaceFormats[formatIndex].format == VK_FORMAT_UNDEFINED ||
		 surfaceFormats[formatIndex].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			srgb = 1;
	}

	VkSurfaceFormatKHR temporaryFormat = {0};
	temporaryFormat.format = bgr ? VK_FORMAT_B8G8R8A8_UNORM : surfaceFormats[0].format;
	temporaryFormat.colorSpace = srgb ? VK_COLOR_SPACE_SRGB_NONLINEAR_KHR : surfaceFormats[0].colorSpace;

	return temporaryFormat;
}

VkPresentModeKHR choosePresentationMode(VkPresentModeKHR *presentModes, uint32_t modeCount)
{
	int8_t mailbox = 0;

	for(uint32_t modeIndex = 0; modeIndex < modeCount; modeIndex++)
		if(presentModes[modeIndex] == VK_PRESENT_MODE_MAILBOX_KHR)
			mailbox = 1;

	if(mailbox)
		return VK_PRESENT_MODE_MAILBOX_KHR;
	else
		return VK_PRESENT_MODE_IMMEDIATE_KHR;
}

VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags flags)
{
	VkImageViewCreateInfo viewInfo = {0};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.image = image;
	viewInfo.format = format;
	viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.subresourceRange.aspectMask = flags;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.layerCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;

	VkImageView imageView;
	printlog(vkCreateImageView(device, &viewInfo, NULL, &imageView) == VK_SUCCESS, __FUNCTION__, NULL);
	return imageView;
}

void createSwapchain()
{
	VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(
	 swapchainDetails.surfaceFormats, swapchainDetails.formatCount);
	VkPresentModeKHR presentMode = choosePresentationMode(
	 swapchainDetails.presentModes, swapchainDetails.modeCount);
	framebufferSize = swapchainDetails.capabilities.minImageCount +
	 (swapchainDetails.capabilities.minImageCount < swapchainDetails.capabilities.maxImageCount);
	swapchainFormat = surfaceFormat.format;
	swapchainExtent = swapchainDetails.capabilities.currentExtent;
	width = swapchainExtent.width;
	height = swapchainExtent.height;

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

	if(graphicsIndex != presentIndex)
	{
		swapchainInfo.queueFamilyIndexCount = 2;
		swapchainInfo.pQueueFamilyIndices = (uint32_t[]){graphicsIndex, presentIndex};
		swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
	}

	printlog(vkCreateSwapchainKHR(device, &swapchainInfo, NULL, &swapchain) == VK_SUCCESS, __FUNCTION__,
	 "Created Swapchain: %s Mode\n", presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "Mailbox" : "Immediate");
	vkGetSwapchainImagesKHR(device, swapchain, &framebufferSize, NULL);
	swapchainImages = malloc(framebufferSize * sizeof(VkImage));
	vkGetSwapchainImagesKHR(device, swapchain, &framebufferSize, swapchainImages);
	printlog(framebufferSize && swapchainImages, __FUNCTION__, "Acquired Swapchain Images\n");

	swapchainViews = malloc(framebufferSize * sizeof(VkImageView));
	for(uint32_t viewIndex = 0; viewIndex < framebufferSize; viewIndex++)
		swapchainViews[viewIndex] =
		 createImageView(swapchainImages[viewIndex], swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	printlog(1, NULL, "Created Swapchain Image Views\n");
}

void createRenderPass()
{
	VkAttachmentReference colorAttachmentReference = {0};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentReference = {0};
	depthAttachmentReference.attachment = 1;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {0};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;
	subpass.pDepthStencilAttachment = &depthAttachmentReference;

	VkAttachmentDescription colorAttachment = {0};
	colorAttachment.format = swapchainFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment = {0};
	depthAttachment.format = chooseDepthFormat();
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDependency dependency = {0};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo = {0};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = (VkAttachmentDescription[]){colorAttachment, depthAttachment};

	printlog(vkCreateRenderPass(device, &renderPassInfo, NULL, &renderPass) == VK_SUCCESS,
	 __FUNCTION__, "Created Render Pass: Subpass Count = %u\n", renderPassInfo.subpassCount);
}

void createDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding uniformBufferBinding = {0};
	uniformBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniformBufferBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uniformBufferBinding.descriptorCount = 1;
	uniformBufferBinding.binding = 0;

	VkDescriptorSetLayoutBinding samplerLayoutBinding = {0};
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.binding = 1;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 2;
	layoutInfo.pBindings = (VkDescriptorSetLayoutBinding[]){uniformBufferBinding, samplerLayoutBinding};

	printlog(vkCreateDescriptorSetLayout(device, &layoutInfo, NULL, &descriptorSetLayout) == VK_SUCCESS,
	 __FUNCTION__, "Created Descriptor Set Layout: Binding Count = %d\n", layoutInfo.bindingCount);
}

VkShaderModule initializeShaderModule(const char *shaderName, const char *filePath)
{
	FILE *file = fopen(filePath, "rb");
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	rewind(file);

	uint32_t *shaderData = calloc(size, 1);
	printlog(fread(shaderData, 1, size, file) == size, __FUNCTION__, NULL);
	fclose(file);

	VkShaderModuleCreateInfo shaderInfo = {0};
	shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderInfo.codeSize = size;
	shaderInfo.pCode = shaderData;

	VkShaderModule shaderModule;
	printlog(vkCreateShaderModule(device, &shaderInfo, NULL, &shaderModule) == VK_SUCCESS,
	 __FUNCTION__, "Created %s Shader Module: %zd bytes\n", shaderName, size);
	free(shaderData);

	return shaderModule;
}

VkVertexInputBindingDescription generateVertexInputBinding()
{
	VkVertexInputBindingDescription inputBinding = {0};
	inputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	inputBinding.binding = 0;
	inputBinding.stride = sizeof(Vertex);
	return inputBinding;
}

VkVertexInputAttributeDescription generatePositionInputAttributes()
{
	VkVertexInputAttributeDescription inputAttribute = {0};
	inputAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	inputAttribute.binding = 0;
	inputAttribute.location = 0;
	inputAttribute.offset = 0;
	return inputAttribute;
}

VkVertexInputAttributeDescription generateColorInputAttributes()
{
	VkVertexInputAttributeDescription inputAttribute = {0};
	inputAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	inputAttribute.binding = 0;
	inputAttribute.location = 1;
	inputAttribute.offset = sizeof(float) * 3;
	return inputAttribute;
}

VkVertexInputAttributeDescription generateTextureInputAttributes()
{
	VkVertexInputAttributeDescription inputAttribute = {0};
	inputAttribute.format = VK_FORMAT_R32G32_SFLOAT;
	inputAttribute.binding = 0;
	inputAttribute.location = 2;
	inputAttribute.offset = sizeof(float) * 6;
	return inputAttribute;
}

void createGraphicsPipeline()
{
	VkShaderModule vertexShader = initializeShaderModule("Vertex", "shaders/vert.spv");
	VkPipelineShaderStageCreateInfo vertexStageInfo = {0};
	vertexStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexStageInfo.module = vertexShader;
	vertexStageInfo.pName = "main";

	VkShaderModule fragmentShader = initializeShaderModule("Fragment", "shaders/frag.spv");
	VkPipelineShaderStageCreateInfo fragmentStageInfo = {0};
	fragmentStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentStageInfo.module = fragmentShader;
	fragmentStageInfo.pName = "main";

	VkVertexInputBindingDescription inputBinding = generateVertexInputBinding();
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {0};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = 3;
	vertexInputInfo.pVertexBindingDescriptions = &inputBinding;
	vertexInputInfo.pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[])
	 {generatePositionInputAttributes(), generateColorInputAttributes(), generateTextureInputAttributes()};

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {0};
	inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {0};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapchainExtent.width;
	viewport.height = (float)swapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkOffset2D offset = {0};
	VkRect2D scissor = {0};
	scissor.offset = offset;
	scissor.extent = swapchainExtent;

	VkPipelineViewportStateCreateInfo viewportStateInfo = {0};
	viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.pViewports = &viewport;
	viewportStateInfo.scissorCount = 1;
	viewportStateInfo.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizerInfo = {0};
	rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerInfo.lineWidth = 1.0f;
	rasterizerInfo.depthClampEnable = VK_FALSE;
	rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
	//rasterizerInfo.cullMode = VK_CULL_MODE_NONE;
	rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizerInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisamplingInfo = {0};
	multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisamplingInfo.sampleShadingEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {0};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_A_BIT |
	 VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendInfo = {0};
	colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendInfo.logicOpEnable = VK_FALSE;
	colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendInfo.attachmentCount = 1;
	colorBlendInfo.pAttachments = &colorBlendAttachment;

	VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {0};
	depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilInfo.depthTestEnable = VK_TRUE;
	depthStencilInfo.depthWriteEnable = VK_TRUE;
	depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilInfo.stencilTestEnable = VK_FALSE;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

	printlog(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &pipelineLayout) == VK_SUCCESS,
	 __FUNCTION__, "Created Pipeline Layout: Set Layout Count = %d\n", pipelineLayoutInfo.setLayoutCount);

	VkGraphicsPipelineCreateInfo pipelineInfo = {0};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = (VkPipelineShaderStageCreateInfo[]){vertexStageInfo, fragmentStageInfo};
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
	pipelineInfo.pViewportState = &viewportStateInfo;
	pipelineInfo.pRasterizationState = &rasterizerInfo;
	pipelineInfo.pMultisampleState = &multisamplingInfo;
	pipelineInfo.pColorBlendState = &colorBlendInfo;
	pipelineInfo.pDepthStencilState = &depthStencilInfo;
	pipelineInfo.layout = pipelineLayout;

	printlog(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &graphicsPipeline)
	 == VK_SUCCESS, __FUNCTION__ , "Created Graphics Pipeline: %u x %u\n", width, height);

	vkDestroyShaderModule(device, fragmentShader, NULL);
	vkDestroyShaderModule(device, vertexShader, NULL);
}

void createCommandPool()
{
	VkCommandPoolCreateInfo poolInfo = {0};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = graphicsIndex;

	printlog(vkCreateCommandPool(device, &poolInfo, NULL, &commandPool) == VK_SUCCESS,
	 __FUNCTION__, "Created Command Pool: Queue Index = %u\n", graphicsIndex);
}

uint32_t chooseMemoryType(uint32_t filter, VkMemoryPropertyFlags flags)
{
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	for(uint32_t index = 0; index < memoryProperties.memoryTypeCount; index++)
		if((filter & (1 << index)) && (memoryProperties.memoryTypes[index].propertyFlags & flags) == flags)
			return index;

	return 0;
}

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
 VkMemoryPropertyFlags properties, VkBuffer *buffer, VkDeviceMemory *bufferMemory)
{
	VkBufferCreateInfo bufferInfo = {0};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferInfo.usage = usage;
	bufferInfo.size = size;

	printlog(vkCreateBuffer(device, &bufferInfo, NULL, buffer) == VK_SUCCESS, __FUNCTION__, NULL);

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, *buffer, &memoryRequirements);

	VkMemoryAllocateInfo allocateInfo = {0};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex = chooseMemoryType(memoryRequirements.memoryTypeBits, properties);

	printlog(vkAllocateMemory(device, &allocateInfo, NULL, bufferMemory) == VK_SUCCESS, __FUNCTION__, NULL);
	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}

VkCommandBuffer beginSingleTimeCommand()
{
	VkCommandBufferAllocateInfo allocateInfo = {0};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandPool = commandPool;
	allocateInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	printlog(vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer) == VK_SUCCESS, __FUNCTION__, NULL);

	VkCommandBufferBeginInfo beginInfo = {0};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);
	return commandBuffer;
}

void endSingleTimeCommand(VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {0};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);
	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommand();

	VkBufferCopy copyRegion = {0};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;

	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
	endSingleTimeCommand(commandBuffer);
}

void createImage(uint32_t imageWidth, uint32_t imageHeight, VkFormat format, VkImageTiling tiling,
 VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image, VkDeviceMemory *memory)
{
	VkImageCreateInfo imageInfo = {0};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.usage = usage;
	imageInfo.extent.width = imageWidth;
	imageInfo.extent.height = imageHeight;
	imageInfo.extent.depth = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.mipLevels = 1;

	printlog(vkCreateImage(device, &imageInfo, NULL, image) == VK_SUCCESS,
	 __FUNCTION__, "Created Image: %u x %u\n", imageWidth, imageHeight);

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(device, *image, &memoryRequirements);

	VkMemoryAllocateInfo allocateInfo = {0};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex = chooseMemoryType(memoryRequirements.memoryTypeBits, properties);

	printlog(vkAllocateMemory(device, &allocateInfo, NULL, memory) == VK_SUCCESS,
	 __FUNCTION__, "Allocated Image Memory: Size = %lu bytes\n", memoryRequirements.size);
	vkBindImageMemory(device, *image, *memory, 0);
}

void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommand();
	VkPipelineStageFlags srcStage = 0, dstStage = 0;

	VkImageMemoryBarrier barrier = {0};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;

	if(newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if(format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT)
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	else
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
	 && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}

	else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	 && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	else if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
	 newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
		 | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}

	else
		printlog(0, __FUNCTION__, NULL);

	vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, NULL, 0, NULL, 1, &barrier);
	endSingleTimeCommand(commandBuffer);
}

void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t imageWidth, uint32_t imageHeight)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommand();

	VkBufferImageCopy region = {0};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageOffset = (VkOffset3D){0, 0, 0};
	region.imageExtent = (VkExtent3D){imageWidth, imageHeight, 1};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	endSingleTimeCommand(commandBuffer);
}

void createDepthBuffer()
{
	VkFormat depthFormat = chooseDepthFormat();

	createImage(swapchainExtent.width, swapchainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
	 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	 &depthImage, &depthMemory);
	depthView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
	transitionImageLayout(depthImage, depthFormat,
	 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	printlog(depthFormat >= 0, __FUNCTION__, "Created Depth Buffer\n");
}

void createFramebuffers()
{
	swapchainFramebuffers = malloc(framebufferSize * sizeof(VkFramebuffer));

	for(uint32_t framebufferIndex = 0; framebufferIndex < framebufferSize; framebufferIndex++)
	{
		VkFramebufferCreateInfo framebufferInfo = {0};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.width = swapchainExtent.width;
		framebufferInfo.height = swapchainExtent.height;
		framebufferInfo.layers = 1;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 2;
		framebufferInfo.pAttachments = (VkImageView[]){swapchainViews[framebufferIndex], depthView};

		printlog(vkCreateFramebuffer(device, &framebufferInfo, NULL, &swapchainFramebuffers[framebufferIndex])
		 == VK_SUCCESS, __FUNCTION__, NULL);
	}

	printlog(1, NULL, "Created Framebuffers: Count = %u\n", framebufferSize);
}

void createTextureImage()
{
	int textureWidth, textureHeight, textureChannels;
	stbi_uc *pixels = stbi_load("textures/chalet.jpg",
	 &textureWidth, &textureHeight, &textureChannels, STBI_rgb_alpha);
	printlog(pixels != NULL, __FUNCTION__, NULL);

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	VkDeviceSize imageSize = textureWidth * textureHeight * 4;

	createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
	 | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, imageSize);
	vkUnmapMemory(device, stagingBufferMemory);
	stbi_image_free(pixels);

	createImage(textureWidth, textureHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
	 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	 &textureImage, &textureMemory);
	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM,
	 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyBufferToImage(stagingBuffer, textureImage, textureWidth, textureHeight);
	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM,
	 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(device, stagingBuffer, NULL);
	vkFreeMemory(device, stagingBufferMemory, NULL);

	textureView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
}

void createTextureSampler()
{
	VkSamplerCreateInfo samplerInfo = {0};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	printlog(vkCreateSampler(device, &samplerInfo, NULL, &textureSampler) == VK_SUCCESS,
	 __FUNCTION__, "Created Texture Sampler\n");
}

void loadObjectModel()
{
	int file = open("models/chalet.obj", O_RDONLY);
	size_t size = lseek(file, 0, SEEK_END);
	lseek(file, 0, SEEK_SET);
	void *data = mmap(NULL, size, PROT_READ, MAP_SHARED, file, 0);
	close(file);

	size_t shapeCount;
	size_t materialCount;
	tinyobj_attrib_t attributes;
	tinyobj_shape_t* shapes;
	tinyobj_material_t* materials;
	printlog(tinyobj_parse_obj(&attributes, &shapes, &shapeCount, &materials, &materialCount, data, size,
	 TINYOBJ_FLAG_TRIANGULATE) == TINYOBJ_SUCCESS, __FUNCTION__, "Read Object File: %lu bytes\n", size);
	munmap(data, size);

	vertexSize = sizeof(Vertex);
	vertexCount = attributes.num_faces;
	vertices = malloc(vertexCount * vertexSize);
	indexSize = sizeof(uint32_t);
	indexCount = vertexCount;
	indices = malloc(indexCount * indexSize);

	for(uint32_t vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++)
	{
		vertices[vertexIndex].pos[0] =  attributes.vertices[3 * attributes.faces[vertexIndex].v_idx];
		vertices[vertexIndex].pos[1] =  attributes.vertices[3 * attributes.faces[vertexIndex].v_idx + 1];
		vertices[vertexIndex].pos[2] = -attributes.vertices[3 * attributes.faces[vertexIndex].v_idx + 2];
		vertices[vertexIndex].tex[0] =  attributes.texcoords[2 * attributes.faces[vertexIndex].vt_idx];
		vertices[vertexIndex].tex[1] = -attributes.texcoords[2 * attributes.faces[vertexIndex].vt_idx + 1] + 1.0f;
		vertices[vertexIndex].col[0] = vertices[vertexIndex].col[1] = vertices[vertexIndex].col[2] = 1.0f;
		indices[vertexIndex] = vertexIndex;
	}

	tinyobj_materials_free(materials, materialCount);
	tinyobj_shapes_free(shapes, shapeCount);
	tinyobj_attrib_free(&attributes);
}

void createVertexBuffer()
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(vertexCount * vertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
	 | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, vertexCount * vertexSize, 0, &data);
	memcpy(data, vertices, vertexCount * vertexSize);
	vkUnmapMemory(device, stagingBufferMemory);

	createBuffer(vertexCount * vertexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertexBuffer, &vertexBufferMemory);
	copyBuffer(stagingBuffer, vertexBuffer, vertexCount * vertexSize);
	printlog(1, NULL, "Created Vertex Buffer: Size = %lu bytes\n", vertexCount * vertexSize);

	vkFreeMemory(device, stagingBufferMemory, NULL);
	vkDestroyBuffer(device, stagingBuffer, NULL);
}

void createIndexBuffer()
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(indexCount * indexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
	 | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, indexCount * indexSize, 0, &data);
	memcpy(data, indices, indexCount * indexSize);
	vkUnmapMemory(device, stagingBufferMemory);

	createBuffer(indexCount * indexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &indexBuffer, &indexBufferMemory);
	copyBuffer(stagingBuffer, indexBuffer, indexCount * indexSize);
	printlog(1, NULL, "Created Index Buffer: Size = %lu bytes\n", indexCount * indexSize);

	vkFreeMemory(device, stagingBufferMemory, NULL);
	vkDestroyBuffer(device, stagingBuffer, NULL);
}

void createUniformBuffers()
{
	uniformBuffers = malloc(framebufferSize * sizeof(VkBuffer));
	uniformBufferMemories = malloc(framebufferSize * sizeof(VkDeviceMemory));

	for(size_t uniformIndex = 0; uniformIndex < framebufferSize; uniformIndex++)
		createBuffer(sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		 &uniformBuffers[uniformIndex], &uniformBufferMemories[uniformIndex]);

	printlog(1, NULL, "Created Uniform Buffers\n");
}

void createDescriptorPool()
{
	VkDescriptorPoolSize uniformBufferSize = {0};
	uniformBufferSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniformBufferSize.descriptorCount = framebufferSize;

	VkDescriptorPoolSize imageSamplerSize = {0};
	imageSamplerSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	imageSamplerSize.descriptorCount = framebufferSize;

	VkDescriptorPoolCreateInfo poolInfo = {0};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = framebufferSize;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = (VkDescriptorPoolSize[]){uniformBufferSize, imageSamplerSize};

	printlog(vkCreateDescriptorPool(device, &poolInfo, NULL, &descriptorPool) == VK_SUCCESS,
	 __FUNCTION__, "Created Descriptor Pool\n");
}

void createDescriptorSets()
{
	VkDescriptorSetLayout *layouts = malloc(framebufferSize * sizeof(VkDescriptorSetLayout));
	for(uint32_t layoutIndex = 0; layoutIndex < framebufferSize; layoutIndex++)
		layouts[layoutIndex] = descriptorSetLayout;

	VkDescriptorSetAllocateInfo descriptorSetInfo = {0};
	descriptorSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetInfo.descriptorPool = descriptorPool;
	descriptorSetInfo.descriptorSetCount = framebufferSize;
	descriptorSetInfo.pSetLayouts = layouts;

	descriptorSets = malloc(framebufferSize * sizeof(VkDescriptorSet));
	printlog(vkAllocateDescriptorSets(device, &descriptorSetInfo, descriptorSets) == VK_SUCCESS,
	 __FUNCTION__, "Allocated Descriptor Sets\n");

	for(uint32_t layoutIndex = 0; layoutIndex < framebufferSize; layoutIndex++)
	{
		VkDescriptorBufferInfo bufferInfo = {0};
		bufferInfo.buffer = uniformBuffers[layoutIndex];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo imageInfo = {0};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = textureView;
		imageInfo.sampler = textureSampler;

		VkWriteDescriptorSet bufferDescriptorWrite = {0};
		bufferDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		bufferDescriptorWrite.dstSet = descriptorSets[layoutIndex];
		bufferDescriptorWrite.dstBinding = 0;
		bufferDescriptorWrite.dstArrayElement = 0;
		bufferDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bufferDescriptorWrite.descriptorCount = 1;
		bufferDescriptorWrite.pBufferInfo = &bufferInfo;

		VkWriteDescriptorSet samplerDescriptorWrite = {0};
		samplerDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		samplerDescriptorWrite.dstSet = descriptorSets[layoutIndex];
		samplerDescriptorWrite.dstBinding = 1;
		samplerDescriptorWrite.dstArrayElement = 0;
		samplerDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerDescriptorWrite.descriptorCount = 1;
		samplerDescriptorWrite.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(device, 2, (VkWriteDescriptorSet[])
		 {bufferDescriptorWrite, samplerDescriptorWrite}, 0, NULL);
	}

	printlog(1, NULL, "Updated Descriptor Sets\n");
}

void createCommandBuffers()
{
	VkCommandBufferAllocateInfo allocateInfo = {0};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = framebufferSize;

	commandBuffers = malloc(framebufferSize * sizeof(VkCommandBuffer));
	printlog(vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers) == VK_SUCCESS,
	 __FUNCTION__, "Allocated Command Buffers\n");

	for(uint32_t commandIndex = 0; commandIndex < framebufferSize; commandIndex++)
	{
		VkCommandBufferBeginInfo beginInfo = {0};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		VkClearValue clearValues[2] = {0};
		clearValues[0].color.float32[0] = 0.0f;
		clearValues[0].color.float32[1] = 0.0f;
		clearValues[0].color.float32[2] = 0.0f;
		clearValues[0].color.float32[3] = 1.0f;
		clearValues[1].depthStencil.depth = 1.0f;
		clearValues[1].depthStencil.stencil = 0;

		VkRenderPassBeginInfo renderPassBeginInfo = {0};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = swapchainFramebuffers[commandIndex];
		renderPassBeginInfo.renderArea.offset = (VkOffset2D){0};
		renderPassBeginInfo.renderArea.extent = swapchainExtent;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		printlog(vkBeginCommandBuffer(commandBuffers[commandIndex], &beginInfo) == VK_SUCCESS, __FUNCTION__, NULL);
		vkCmdBeginRenderPass(commandBuffers[commandIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffers[commandIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
		vkCmdBindDescriptorSets(commandBuffers[commandIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
		 pipelineLayout, 0, 1, &descriptorSets[commandIndex], 0, NULL);
		vkCmdBindVertexBuffers(commandBuffers[commandIndex], 0, 1, &vertexBuffer, (VkDeviceSize[]){0});
		vkCmdBindIndexBuffer(commandBuffers[commandIndex], indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffers[commandIndex], indexCount, 1, 0, 0, 0);
		vkCmdEndRenderPass(commandBuffers[commandIndex]);
		printlog(vkEndCommandBuffer(commandBuffers[commandIndex]) == VK_SUCCESS, __FUNCTION__, NULL);
	}

	printlog(1, NULL, "Recorded Commands\n");
}

void createSyncObjects()
{
	framebufferLimit = 2;

	imageAvailable = malloc(framebufferLimit * sizeof(VkSemaphore));
	renderFinished = malloc(framebufferLimit * sizeof(VkSemaphore));
	frameFences = malloc(framebufferLimit * sizeof(VkFence));

	VkSemaphoreCreateInfo semaphoreInfo = {0};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo = {0};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for(uint32_t syncIndex = 0; syncIndex < framebufferLimit; syncIndex++)
	{
		vkCreateSemaphore(device, &semaphoreInfo, NULL, &imageAvailable[syncIndex]);
		vkCreateSemaphore(device, &semaphoreInfo, NULL, &renderFinished[syncIndex]);
		vkCreateFence(device, &fenceInfo, NULL, &frameFences[syncIndex]);
	}

	printlog(1, NULL, "Created Syncronization Objects\n");
}

void recreateSwapchain()
{
	vkDeviceWaitIdle(device);
	cleanupSwapchain();
	swapchainDetails =
	 generateSwapchainDetails(physicalDevice);

	createSwapchain();
	createRenderPass();
	createGraphicsPipeline();
	createDepthBuffer();
	createFramebuffers();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createCommandBuffers();
}

void setup()
{
	createInstance();
	registerMessenger();
	createWindow();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapchain();
	createRenderPass();
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createCommandPool();
	createDepthBuffer();
	createFramebuffers();
	createTextureImage();
	createTextureSampler();
	loadObjectModel();
	createVertexBuffer();
	createIndexBuffer();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createCommandBuffers();
	createSyncObjects();
}

inline xcb_atom_t windowEvent()
{
	event = xcb_poll_for_event(xconn);

	if(event)
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
			return ((xcb_client_message_event_t*)event)->data.data32[0];

		free(event);
	}

	return !destroyEvent;
}

void identity(float m[])
{
	m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void normalize(float v[])
{
	const float eps = 0.0009765625f; //Epsilon = 2 ^ -10
	float mag = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

	if(mag > eps && fabsf(1.0f - mag) > eps)
	{
		v[0] /= mag;
		v[1] /= mag;
		v[2] /= mag;
	}
}

float dot(float a[], float b[])
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void cross(float a[], float b[], float c[])
{
	c[0] = a[1] * b[2] - a[2] * b[1];
	c[1] = a[2] * b[0] - a[0] * b[2];
	c[2] = a[0] * b[1] - a[1] * b[0];
}

void multiply(float a[], float b[], float c[])
{
	for(int row = 0; row < 4; row++)
		for(int col = 0; col < 4; col++)
			for(int itr = 0; itr < 4; itr++)
				c[4 * row + col] += a[4 * row + itr] * b[4 * itr + col];
}

void scale(float m[], float v[])
{
	float k[] = {
		v[0], 0.0f, 0.0f, 0.0f,
		0.0f, v[1], 0.0f, 0.0f,
		0.0f, 0.0f, v[2], 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}, t[16] = {0};

	multiply(k, m, t);
	memcpy(m, t, sizeof(t));
}

void translate(float m[], float v[])
{
	float k[] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		v[0], v[1], v[2], 1.0f
	}, t[16] = {0};

 	multiply(k, m, t);
	memcpy(m, t, sizeof(t));
}

void rotate(float m[], float v[], float r)
{
	normalize(v);

	float x = v[0], y = v[1], z = v[2];
	float xx = x * x, xy = x * y, xz = x * z;
	float yy = y * y, yz = y * z, zz = z * z;
	float s = sinf(r), c = cosf(r), w = 1 - cosf(r);

	float k[] = {
		w * xx + c,     w * xy + s * z, w * xz - s * y, 0.0f,
		w * xy - s * z, w * yy + c,     w * yz + s * x, 0.0f,
		w * xz + s * y, w * yz - s * x, w * zz + c,     0.0f,
		0.0f,           0.0f,           0.0f,           1.0f
	}, t[16] = {0};

 	multiply(k, m, t);
	memcpy(m, t, sizeof(t));
}

void camera(float m[], float eye[], float cent[], float top[])
{
	float fwd[] = {cent[0] - eye[0], cent[1] - eye[1], cent[2] - eye[2]};
	normalize(fwd);

	float left[3];
	cross(fwd, top, left);
	normalize(left);

	float up[3];
	cross(left, fwd, up);

	float k[] = {
		 left[0],         up[0],          -fwd[0],          0.0f,
		 left[1],         up[1],          -fwd[1],          0.0f,
		 left[2],         up[2],          -fwd[2],          0.0f,
		-dot(left, eye), -dot(up, eye),    dot(fwd, eye),   1.0f
	};

	memcpy(m, k, sizeof(k));
}

void orthographic(float m[], float h, float r, float n, float f)
{
	float k[] = {
		2 / (h * r),  0.0f,         0.0f,         0.0f,
		0.0f,        -2 / h,        0.0f,         0.0f,
		0.0f,         0.0f,         1 / (n - f),  0.0f,
		0.0f,         0.0f,         n / (n - f),  1.0f
	};

	memcpy(m, k, sizeof(k));
}

void frustum(float m[], float h, float r, float n, float f)
{
	float k[] = {
		2 * n / (h * r),  0.0f,             0.0f,             0.0f,
		0.0f,            -2 * n / h,        0.0f,             0.0f,
		0.0f,             0.0f,             f / (n - f),     -1.0f,
		0.0f,             0.0f,             n * f / (n - f),  0.0f
	};

	memcpy(m, k, sizeof(k));
}

void perspective(float m[], float fov, float asp, float n, float f)
{
	float t = tanf(fov / 2);

	float k[] = {
		1 / (t * asp),    0.0f,             0.0f,             0.0f,
		0.0f,            -1 / t,            0.0f,             0.0f,
		0.0f,             0.0f,             f / (n - f),     -1.0f,
		0.0f,             0.0f,             n * f / (n - f),  0.0f
	};

	memcpy(m, k, sizeof(k));
}

void updateUniformBuffer(int index)
{
	static float theta = 0.0f;
	static long checkPoint = 0L;
	UniformBufferObject ubo = {0};

	long timediff = (timespec.tv_nsec - checkPoint);
	checkPoint = timespec.tv_nsec;
	if(timediff < 0)
		timediff += 1e9L;
	theta += PI * timediff / 4e9L;

	identity(ubo.model);
	rotate(ubo.model, (float[]){0.0f, 0.0f, -1.0f}, theta);
	camera(ubo.view, (float[]){0.0f, -1.5f, -1.0f}, (float[]){0.0f, 0.0f, 0.0f}, (float[]){0.0f, 0.0f, -1.0f});
	perspective(ubo.proj, PI / 2, (float)swapchainExtent.width / (float)swapchainExtent.height, 0.1f, 10.0f);

	void *data;
	vkMapMemory(device, uniformBufferMemories[index], 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(device, uniformBufferMemories[index]);
}

void draw()
{
	time_t currentTime = 0;
	uint32_t currentFrame = 0, frameCount = 0, checkPoint = 0;
	printlog(1, NULL, "Started Drawing...\n");

	while(windowEvent() != destroyEvent)
	{
		vkWaitForFences(device, 1, &frameFences[currentFrame], VK_TRUE, ULONG_MAX);

		uint32_t imageIndex;
		VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, ULONG_MAX,
		 imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);
		if(acquireResult == VK_SUBOPTIMAL_KHR || acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapchain();
			continue;
		}

		clock_gettime(CLOCK_REALTIME, &timespec);
		updateUniformBuffer(imageIndex);

		VkSemaphore waitSemaphores[] = {imageAvailable[currentFrame]};
		VkSemaphore signalSemaphores[] = {renderFinished[currentFrame]};

		VkSubmitInfo submitInfo = {0};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
		submitInfo.pWaitDstStageMask = (VkPipelineStageFlags[]){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

		vkResetFences(device, 1, &frameFences[currentFrame]);
		vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameFences[currentFrame]);

		VkPresentInfoKHR presentInfo = {0};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = (VkSwapchainKHR[]){swapchain};

		VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
		if(presentResult == VK_SUBOPTIMAL_KHR || presentResult == VK_ERROR_OUT_OF_DATE_KHR)
			recreateSwapchain();

		currentFrame = ++frameCount % framebufferLimit;
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

	printlog(1, NULL, "Drawing Ended\n");
	vkDeviceWaitIdle(device);
}

void cleanupSwapchain()
{
	vkDestroyImageView(device, depthView, NULL);
	vkDestroyImage(device, depthImage, NULL);
	vkFreeMemory(device, depthMemory, NULL);
	for(uint32_t framebufferIndex = 0; framebufferIndex < framebufferSize; framebufferIndex++)
		vkDestroyFramebuffer(device, swapchainFramebuffers[framebufferIndex], NULL);
	vkFreeCommandBuffers(device, commandPool, framebufferSize, commandBuffers);
	vkDestroyPipeline(device, graphicsPipeline, NULL);
	vkDestroyPipelineLayout(device, pipelineLayout, NULL);
	vkDestroyRenderPass(device, renderPass, NULL);
	for(uint32_t viewIndex = 0; viewIndex < framebufferSize; viewIndex++)
		vkDestroyImageView(device, swapchainViews[viewIndex], NULL);
	vkDestroySwapchainKHR(device, swapchain, NULL);
	vkDestroyDescriptorPool(device, descriptorPool, NULL);
	for(size_t uniformIndex = 0; uniformIndex < framebufferSize; uniformIndex++)
	{
		vkDestroyBuffer(device, uniformBuffers[uniformIndex], NULL);
		vkFreeMemory(device, uniformBufferMemories[uniformIndex], NULL);
	}
	free(swapchainDetails.presentModes);
	free(swapchainDetails.surfaceFormats);
}

void clean()
{
	printlog(1, NULL, "Started Cleaning\n");
	for(uint32_t syncIndex = 0; syncIndex < framebufferLimit; syncIndex++)
	{
		vkDestroyFence(device, frameFences[syncIndex], NULL);
		vkDestroySemaphore(device, renderFinished[syncIndex], NULL);
		vkDestroySemaphore(device, imageAvailable[syncIndex], NULL);
	}
	vkDestroyDescriptorPool(device, descriptorPool, NULL);
	for(size_t uniformIndex = 0; uniformIndex < framebufferSize; uniformIndex++)
	{
		vkDestroyBuffer(device, uniformBuffers[uniformIndex], NULL);
		vkFreeMemory(device, uniformBufferMemories[uniformIndex], NULL);
	}
	vkDestroyBuffer(device, indexBuffer, NULL);
	vkFreeMemory(device, indexBufferMemory, NULL);
	vkDestroyBuffer(device, vertexBuffer, NULL);
	vkFreeMemory(device, vertexBufferMemory, NULL);
	vkDestroySampler(device, textureSampler, NULL);
	vkDestroyImageView(device, textureView, NULL);
	vkDestroyImage(device, textureImage, NULL);
	vkFreeMemory(device, textureMemory, NULL);
	vkDestroyImageView(device, depthView, NULL);
	vkDestroyImage(device, depthImage, NULL);
	vkFreeMemory(device, depthMemory, NULL);
	for(uint32_t framebufferIndex = 0; framebufferIndex < framebufferSize; framebufferIndex++)
		vkDestroyFramebuffer(device, swapchainFramebuffers[framebufferIndex], NULL);
	vkFreeCommandBuffers(device, commandPool, framebufferSize, commandBuffers);
	vkDestroyCommandPool(device, commandPool, NULL);
	vkDestroyPipeline(device, graphicsPipeline, NULL);
	vkDestroyPipelineLayout(device, pipelineLayout, NULL);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, NULL);
	vkDestroyRenderPass(device, renderPass, NULL);
	for(uint32_t viewIndex = 0; viewIndex < framebufferSize; viewIndex++)
		vkDestroyImageView(device, swapchainViews[viewIndex], NULL);
	vkDestroySwapchainKHR(device, swapchain, NULL);
	vkDestroyDevice(device, NULL);
	vkDestroySurfaceKHR(instance, surface, NULL);
	xcb_destroy_window(xconn, window);
	xcb_disconnect(xconn);
	PFN_vkDestroyDebugUtilsMessengerEXT destroyMessenger =
	 (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if(destroyMessenger != NULL)
		destroyMessenger(instance, messenger, NULL);
	vkDestroyInstance(instance, NULL);
	printlog(1, NULL, "Cleaned Up!\n");
}

int main()
{
	setup();
	draw();
	clean();
}
