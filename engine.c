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

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define STBI_ASSERT(x)
#define STB_IMAGE_IMPLEMENTATION
#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "libraries/stb_image.h"
#include "libraries/tinyobj_loader_c.h"

union vertex
{
	struct
	{
		float pos[3];
		float col[3];
		float tex[2];
	};

	uint16_t data[16];
};

struct node
{
	int size, limit;
	uint32_t *indices;
	union vertex **vertices;
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

typedef union vertex Vertex;
typedef struct node Node;
typedef struct uniformBufferObject UniformBufferObject;
typedef struct swapchainDetails SwapchainDetails;

GLFWwindow* window;
int width, height, focus, ready;
int keyW, keyA, keyS, keyD, keyR, keyF;
double moveX, moveY, mouseX, mouseY;
int fillMode, cullMode;
float up[4], forward[4], position[4];
Node *hashMap;
struct timespec timespec, timeorig;

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
VkDeviceSize vertexCount, vertexLimit, vertexSize;
VkDeviceSize indexCount, indexLimit, indexSize;
Vertex *vertices;
uint32_t *indices;
VkBuffer vertexBuffer, indexBuffer;
VkDeviceMemory vertexBufferMemory, indexBufferMemory;
VkBuffer *uniformBuffers;
VkDeviceMemory *uniformBufferMemories;
VkCommandPool commandPool;
VkCommandBuffer *commandBuffers;
uint32_t mipLevels;
VkImage textureImage, depthImage, colorImage;
VkImageView textureView, depthView, colorView;
VkDeviceMemory textureMemory, depthMemory, colorMemory;
VkSampler textureSampler;
VkSampleCountFlagBits msaaSamples;
VkDescriptorPool descriptorPool;
VkDescriptorSet *descriptorSets;
VkSemaphore *imageAvailable, *renderFinished;
VkFence *frameFences;

void setup();
void clean();
void recreateSwapchain();
void cleanupSwapchain();
void recreatePipeline();
void cleanupPipeline();

void printlog(int success, const char *format, ...)
{
	if(format)
	{
		char timestamp[20];
		clock_gettime(CLOCK_REALTIME, &timespec);
		strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", localtime(&(time_t){timespec.tv_sec}));
		printf("%s %7.3Lf %c: ", timestamp, timespec.tv_nsec / 1e6L, success ? 'S' : 'F');
		va_list arguments;
		va_start(arguments, format);
		vprintf(format, arguments);
		va_end(arguments);
		printf("\n");
	}

	if(!success)
		exit(1);
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

	uint32_t extensionCount;
	const char **extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
	const char **extensionNames = malloc((extensionCount + 1) * sizeof(char*));
	memcpy(extensionNames, extensions, extensionCount * sizeof(char*));
	extensions[extensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	extensionCount++;
	
	VkInstanceCreateInfo instanceInfo = {0};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.enabledLayerCount = 1;
	instanceInfo.ppEnabledLayerNames = (const char*[]){"VK_LAYER_LUNARG_standard_validation"};
	instanceInfo.enabledExtensionCount = extensionCount;
	instanceInfo.ppEnabledExtensionNames = extensions;

	printlog(vkCreateInstance(&instanceInfo, NULL, &instance) == VK_SUCCESS, "Create Vulkan Instance");
}

static VKAPI_ATTR VkBool32 VKAPI_CALL messageCallback(
 VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
 const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
	(void)type;
	(void)severity;
	(void)pUserData;

	printlog(severity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, pCallbackData->pMessage);
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
	 "Register Validation Layer Messenger");
}

void keyEvent(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	(void)mods;
	(void)scancode;

	if(focus)
	{
		if(action == GLFW_RELEASE)
		{
			if(key == GLFW_KEY_W)
				keyW = 0;
			else if(key == GLFW_KEY_S)
				keyS = 0;
			else if(key == GLFW_KEY_A)
				keyA = 0;
			else if(key == GLFW_KEY_D)
				keyD = 0;
			else if(key == GLFW_KEY_R)
				keyR = 0;
			else if(key == GLFW_KEY_F)
				keyF = 0;
		}

		else if(action == GLFW_PRESS)
		{
			if(key == GLFW_KEY_W)
				keyW = 1;
			else if(key == GLFW_KEY_S)
				keyS = 1;
			else if(key == GLFW_KEY_A)
				keyA = 1;
			else if(key == GLFW_KEY_D)
				keyD = 1;
			else if(key == GLFW_KEY_R)
				keyR = 1;
			else if(key == GLFW_KEY_F)
				keyF = 1;
			else if(key == GLFW_KEY_C)
			{
				cullMode = !cullMode;
				recreatePipeline();
			}
			else if(key == GLFW_KEY_V)
			{
				fillMode = !fillMode;
				recreatePipeline();
			}
			else if(key == GLFW_KEY_ESCAPE)
			{
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				focus = keyW = keyS = keyA = keyD = keyR = keyF = 0;
			}
		}
	}

	else if(action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwGetCursorPos(window, &mouseX, &mouseY);
		focus = 1;
	}
}

void mouseEvent(GLFWwindow* window, double x, double y)
{
	(void)window;

	if(glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
	{
		moveX = x - mouseX;
		moveY = y - mouseY;
		mouseX = x;
		mouseY = y;
	}
}

void resizeEvent(GLFWwindow* window, int w, int h)
{
	(void)window;

	width = w;
	height = h;
}

void createSurface()
{
	focus = 1;
	width = 800;
	height = 600;
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(width, height, "Vulkan", NULL, NULL);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwGetCursorPos(window, &mouseX, &mouseY);
	glfwSetKeyCallback(window, keyEvent);
	glfwSetCursorPosCallback(window, mouseEvent);
	glfwSetFramebufferSizeCallback(window, resizeEvent);
	printlog(glfwCreateWindowSurface(instance, window, NULL, &surface) == VK_SUCCESS, "Create GLFW Vulkan Surface");
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
	uint32_t deviceCount;
	int32_t maxScore = -1, bestIndex = -1;
	VkSampleCountFlags bestSample = VK_SAMPLE_COUNT_1_BIT;
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

		VkSampleCountFlags sampleCount =
		 deviceProperties.limits.framebufferColorSampleCounts < deviceProperties.limits.framebufferDepthSampleCounts ?
		 deviceProperties.limits.framebufferColorSampleCounts : deviceProperties.limits.framebufferDepthSampleCounts;

		for(int sampleShift = 6; sampleShift >= 0; sampleShift--)
		{
			if(sampleCount & (VK_SAMPLE_COUNT_1_BIT << sampleShift))
			{
				sampleCount = VK_SAMPLE_COUNT_1_BIT << sampleShift;
				break;
			}
		}

		if(formatCount && modeCount && swapchainSupport &&
		 deviceFeatures.geometryShader && deviceFeatures.samplerAnisotropy)
		{
			int32_t deviceScore = extensionCount + (formatCount + modeCount) * 16 + sampleCount;
			if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				deviceScore *= 2;

			if(deviceScore >= maxScore)
			{
				maxScore = deviceScore;
				bestIndex = deviceIndex;
				bestSample = sampleCount;
				strncpy(deviceName, deviceProperties.deviceName, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);
			}
		}
	}

	printlog(maxScore != -1, "Pick Physical Device: %s", deviceName);
	physicalDevice = devices[bestIndex];
	msaaSamples = bestSample;
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
	deviceFeatures.sampleRateShading = VK_TRUE;
	deviceFeatures.fillModeNonSolid = VK_TRUE;

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

	printlog(vkCreateDevice(physicalDevice, &deviceInfo, NULL, &device) == VK_SUCCESS,
	 "Create Logical Device: %s Queues", queueCount == 1 ? "Common" : "Specialized");
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

VkImageView createImageView(VkImage image, uint32_t levels, VkFormat format, VkImageAspectFlags flags)
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
	viewInfo.subresourceRange.levelCount = levels;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.layerCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;

	VkImageView imageView;
	printlog(vkCreateImageView(device, &viewInfo, NULL, &imageView) == VK_SUCCESS, NULL);
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

	printlog(vkCreateSwapchainKHR(device, &swapchainInfo, NULL, &swapchain) == VK_SUCCESS,
	 "Create Swapchain: %s Mode", presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "Mailbox" : "Immediate");
	vkGetSwapchainImagesKHR(device, swapchain, &framebufferSize, NULL);
	swapchainImages = malloc(framebufferSize * sizeof(VkImage));
	vkGetSwapchainImagesKHR(device, swapchain, &framebufferSize, swapchainImages);
	printlog(framebufferSize && swapchainImages, "Acquire Swapchain Images");

	swapchainViews = malloc(framebufferSize * sizeof(VkImageView));
	for(uint32_t viewIndex = 0; viewIndex < framebufferSize; viewIndex++)
		swapchainViews[viewIndex] =
		 createImageView(swapchainImages[viewIndex], 1, swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	printlog(1, "Create Swapchain Image Views");
}

void createRenderPass()
{
	VkAttachmentReference colorAttachmentReference = {0};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentReference = {0};
	depthAttachmentReference.attachment = 1;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorResolveReference = {0};
	colorResolveReference.attachment = 2;
	colorResolveReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {0};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;
	subpass.pDepthStencilAttachment = &depthAttachmentReference;
	subpass.pResolveAttachments = &colorResolveReference;

	VkAttachmentDescription colorAttachment = {0};
	colorAttachment.format = swapchainFormat;
	colorAttachment.samples = msaaSamples;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment = {0};
	depthAttachment.format = chooseDepthFormat();
	depthAttachment.samples = msaaSamples;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription colorResolve = {0};
	colorResolve.format = swapchainFormat;
	colorResolve.samples = VK_SAMPLE_COUNT_1_BIT;
	colorResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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
	renderPassInfo.attachmentCount = 3;
	renderPassInfo.pAttachments = (VkAttachmentDescription[]){colorAttachment, depthAttachment, colorResolve};

	printlog(vkCreateRenderPass(device, &renderPassInfo, NULL, &renderPass) == VK_SUCCESS,
	 "Create Render Pass: Subpass Count = %u", renderPassInfo.subpassCount);
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
	 "Create Descriptor Set Layout: Binding Count = %d", layoutInfo.bindingCount);
}

VkShaderModule initializeShaderModule(const char *shaderName, const char *filePath)
{
	FILE *file = fopen(filePath, "rb");
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	rewind(file);

	uint32_t *shaderData = calloc(size, 1);
	printlog(fread(shaderData, 1, size, file) == size, NULL);
	fclose(file);

	VkShaderModuleCreateInfo shaderInfo = {0};
	shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderInfo.codeSize = size;
	shaderInfo.pCode = shaderData;

	VkShaderModule shaderModule;
	printlog(vkCreateShaderModule(device, &shaderInfo, NULL, &shaderModule) == VK_SUCCESS,
	 "Create %s Shader Module: %zd bytes", shaderName, size);
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
	rasterizerInfo.depthBiasEnable = VK_FALSE;
	rasterizerInfo.depthClampEnable = VK_FALSE;
	rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizerInfo.polygonMode = fillMode ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	rasterizerInfo.cullMode = cullMode ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

	VkPipelineMultisampleStateCreateInfo multisamplingInfo = {0};
	multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingInfo.sampleShadingEnable = VK_TRUE;
	multisamplingInfo.minSampleShading = 1.0f;
	multisamplingInfo.rasterizationSamples = msaaSamples;

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
	 "Create Pipeline Layout: Set Layout Count = %d", pipelineLayoutInfo.setLayoutCount);

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
	 == VK_SUCCESS, "Create Graphics Pipeline: %u x %u", width, height);

	vkDestroyShaderModule(device, fragmentShader, NULL);
	vkDestroyShaderModule(device, vertexShader, NULL);
}

void createCommandPool()
{
	VkCommandPoolCreateInfo poolInfo = {0};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = graphicsIndex;

	printlog(vkCreateCommandPool(device, &poolInfo, NULL, &commandPool) == VK_SUCCESS,
	 "Create Command Pool: Queue Index = %u", graphicsIndex);
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

	printlog(vkCreateBuffer(device, &bufferInfo, NULL, buffer) == VK_SUCCESS, NULL);

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, *buffer, &memoryRequirements);

	VkMemoryAllocateInfo allocateInfo = {0};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex = chooseMemoryType(memoryRequirements.memoryTypeBits, properties);

	printlog(vkAllocateMemory(device, &allocateInfo, NULL, bufferMemory) == VK_SUCCESS, NULL);
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
	printlog(vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer) == VK_SUCCESS, NULL);

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

void createImage(uint32_t imageWidth, uint32_t imageHeight, uint32_t levels, VkSampleCountFlagBits samples,
 VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
 VkImage *image, VkDeviceMemory *memory)
{
	VkImageCreateInfo imageInfo = {0};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = samples;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.usage = usage;
	imageInfo.extent.width = imageWidth;
	imageInfo.extent.height = imageHeight;
	imageInfo.mipLevels = levels;
	imageInfo.arrayLayers = 1;
	imageInfo.extent.depth = 1;

	printlog(vkCreateImage(device, &imageInfo, NULL, image) == VK_SUCCESS,
	 "Create Image: %u x %u", imageWidth, imageHeight);

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(device, *image, &memoryRequirements);

	VkMemoryAllocateInfo allocateInfo = {0};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex = chooseMemoryType(memoryRequirements.memoryTypeBits, properties);

	printlog(vkAllocateMemory(device, &allocateInfo, NULL, memory) == VK_SUCCESS,
	 "Allocate Image Memory: Size = %lu bytes", memoryRequirements.size);
	vkBindImageMemory(device, *image, *memory, 0);
}

void transitionImageLayout(VkImage image, uint32_t levels, VkFormat format, VkImageLayout layout)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommand();

	VkImageMemoryBarrier barrier = {0};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.levelCount = levels;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
 	barrier.srcAccessMask = 0;

	VkPipelineStageFlags stage = 0;

	if(layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}

	else if(layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}

	else if(layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT |
		 (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT
		 ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
		 | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}

	else
		printlog(0, NULL);

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
	 stage, 0, 0, NULL, 0, NULL, 1, &barrier);
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

void createColorBuffer()
{
	createImage(swapchainExtent.width, swapchainExtent.height, 1, msaaSamples, swapchainFormat,
	 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
	 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &colorImage, &colorMemory);
	colorView = createImageView(colorImage, 1, swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	transitionImageLayout(colorImage, 1, swapchainFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	printlog(1, "Create Color Buffer: %dx MSAA", msaaSamples);
}

void createDepthBuffer()
{
	VkFormat depthFormat = chooseDepthFormat();

	createImage(swapchainExtent.width, swapchainExtent.height, 1, msaaSamples, depthFormat,
	 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	 &depthImage, &depthMemory);
	depthView = createImageView(depthImage, 1, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
	transitionImageLayout(depthImage, 1, depthFormat, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	printlog(depthFormat >= 0, "Create Depth Buffer");
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
		framebufferInfo.attachmentCount = 3;
		framebufferInfo.pAttachments = (VkImageView[]){colorView, depthView, swapchainViews[framebufferIndex]};

		printlog(vkCreateFramebuffer(device, &framebufferInfo, NULL, &swapchainFramebuffers[framebufferIndex])
		 == VK_SUCCESS, NULL);
	}

	printlog(1, "Create Framebuffers: Count = %u", framebufferSize);
}

void generateMipmaps(VkImage image, int32_t width, int32_t height, uint32_t levels, VkFormat format)
{
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
	printlog(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT, NULL);

	VkCommandBuffer commandBuffer = beginSingleTimeCommand();

	VkImageMemoryBarrier barrier = {0};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.image = image;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = width, mipHeight = height;
	for (uint32_t mipLevel = 1; mipLevel < levels; mipLevel++)
	{
		barrier.subresourceRange.baseMipLevel = mipLevel - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		 0, 0, NULL, 0, NULL, 1, &barrier);

		VkImageBlit blit = {0};
		blit.srcOffsets[0] = (VkOffset3D){0, 0, 0};
		blit.srcOffsets[1] = (VkOffset3D){mipWidth, mipHeight, 1};
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = mipLevel - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = (VkOffset3D){0, 0, 0};
		blit.dstOffsets[1] = (VkOffset3D){mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = mipLevel;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		 image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		 0, 0, NULL, 0, NULL, 1, &barrier);

		if(mipWidth > 1)
			mipWidth /= 2;
		if(mipHeight > 1)
			mipHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = levels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
	 0, 0, NULL, 0, NULL, 1, &barrier);

	endSingleTimeCommand(commandBuffer);

	printlog(1, "Generate Mipmaps: %d Levels", levels);
}

void createTextureImage()
{
	int textureWidth, textureHeight, textureChannels;
	stbi_uc *pixels = stbi_load("textures/chalet.jpg",
	 &textureWidth, &textureHeight, &textureChannels, STBI_rgb_alpha);
	printlog(pixels != NULL, NULL);
	mipLevels = floor(log2f(fmaxf(textureWidth, textureHeight))) + 1;

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

	createImage(textureWidth, textureHeight, mipLevels, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM,
	 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
	 VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &textureImage, &textureMemory);
	transitionImageLayout(textureImage, mipLevels, VK_FORMAT_R8G8B8A8_UNORM,
	 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyBufferToImage(stagingBuffer, textureImage, textureWidth, textureHeight);
	generateMipmaps(textureImage, textureWidth, textureHeight, mipLevels, VK_FORMAT_R8G8B8A8_UNORM);

	vkDestroyBuffer(device, stagingBuffer, NULL);
	vkFreeMemory(device, stagingBufferMemory, NULL);

	textureView = createImageView(textureImage, mipLevels, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
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
	samplerInfo.maxLod = (float)mipLevels;

	printlog(vkCreateSampler(device, &samplerInfo, NULL, &textureSampler) == VK_SUCCESS, "Create Texture Sampler");
}

uint16_t hashVertex(Vertex vertex)
{
	uint16_t hash = 0;
	for(uint16_t seed = 0; seed < 16; seed++)
		hash ^= (vertex.data[seed] << seed) | (vertex.data[seed] >> (16 - seed));
	return hash;
}

int compareVertex(Vertex v1, Vertex v2)
{
	return v1.pos[0] == v2.pos[0] && v1.pos[1] == v2.pos[1] && v1.pos[2] == v2.pos[2]
	 && v1.col[0] == v2.col[0] && v1.col[1] == v2.col[1] && v1.col[2] == v2.col[2] &&
	 v1.tex[0] == v2.tex[0] && v1.tex[1] == v2.tex[1];
}

void loadObject(const char *model, float *origin)
{
	int file = open(model, O_RDONLY);
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
	 TINYOBJ_FLAG_TRIANGULATE) == TINYOBJ_SUCCESS, "Read Object File: %lu bytes", size);
	munmap(data, size);

	uint32_t uniqueCount = 0;

	for(uint32_t index = 0; index < attributes.num_faces; index++)
	{
		Vertex vertex = {{{
				origin[0] + attributes.vertices[3 * attributes.faces[index].v_idx],
				origin[1] + attributes.vertices[3 * attributes.faces[index].v_idx + 1],
				origin[2] - attributes.vertices[3 * attributes.faces[index].v_idx + 2]
			},{1.0f, 1.0f, 1.0f},{
				 attributes.texcoords[2 * attributes.faces[index].vt_idx],
				-attributes.texcoords[2 * attributes.faces[index].vt_idx + 1]
		}}};

		uint16_t iterator = 0, hash = hashVertex(vertex);

		while(iterator < hashMap[hash].size && !compareVertex(vertex, *hashMap[hash].vertices[iterator]))
			iterator++;

		if(iterator == hashMap[hash].size)
		{
			if(!hashMap[hash].limit)
			{
				hashMap[hash].limit = 128;
				hashMap[hash].indices = malloc(hashMap[hash].limit * sizeof(uint32_t));
				hashMap[hash].vertices = malloc(hashMap[hash].limit * sizeof(Vertex*));
			}

			else if(hashMap[hash].size == hashMap[hash].limit)
			{
				hashMap[hash].limit *= 2;
				hashMap[hash].indices = realloc(hashMap[hash].indices, hashMap[hash].limit * sizeof(uint32_t));
				hashMap[hash].vertices = realloc(hashMap[hash].vertices, hashMap[hash].limit * sizeof(Vertex*));
			}

			if(vertexCount + uniqueCount == vertexLimit)
			{
				vertexLimit *= 2;
				vertices = realloc(vertices, vertexLimit * vertexSize);
			}

			indices[indexCount + index] = hashMap[hash].indices[iterator] = vertexCount + uniqueCount;
			vertices[vertexCount + uniqueCount] = vertex;
			hashMap[hash].vertices[iterator] = &vertices[vertexCount + uniqueCount];
			uniqueCount++;
			hashMap[hash].size++;
		}

		else
			indices[indexCount + index] = hashMap[hash].indices[iterator];
	}

	vertexCount += uniqueCount;
	indexCount += attributes.num_faces;

	tinyobj_materials_free(materials, materialCount);
	tinyobj_shapes_free(shapes, shapeCount);
	tinyobj_attrib_free(&attributes);
}

void createObjectModels()
{
	vertexCount = 0;
	vertexLimit = INT_MAX / 1024;
	vertexSize = sizeof(Vertex);
	vertices = malloc(vertexLimit * vertexSize);

	indexCount = 0;
	indexLimit = INT_MAX / 256;
	indexSize = sizeof(uint32_t);
	indices = malloc(indexLimit * indexSize);

	hashMap = calloc(USHRT_MAX + 1, sizeof(Node));

	loadObject("models/chalet.obj", (float[]){-1.0f, -1.0f, 0.0f});
	loadObject("models/chalet.obj", (float[]){-1.0f, 1.0f, 0.0f});
	loadObject("models/chalet.obj", (float[]){1.0f, -1.0f, 0.0f});
	loadObject("models/chalet.obj", (float[]){1.0f, 1.0f, 0.0f});

	vertices[vertexCount + 0] = (Vertex){{{-10.0f, -10.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.03125f, 0.84375f}}};
	vertices[vertexCount + 1] = (Vertex){{{ 10.0f, -10.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.125f,   0.84375f}}};
	vertices[vertexCount + 2] = (Vertex){{{ 10.0f,  10.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.125f,   0.9375f}}};
	vertices[vertexCount + 3] = (Vertex){{{-10.0f,  10.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.03125f, 0.9375f}}};

	indices[indexCount + 0] = vertexCount;
	indices[indexCount + 1] = vertexCount + 1;
	indices[indexCount + 2] = vertexCount + 2;
	indices[indexCount + 3] = vertexCount;
	indices[indexCount + 4] = vertexCount + 2;
	indices[indexCount + 5] = vertexCount + 3;

	vertexCount += 4;
	indexCount += 6;

	vertices = realloc(vertices, vertexCount * vertexSize);
	indices = realloc(indices, indexCount * indexSize);

	for(uint32_t index = 0; index < USHRT_MAX + 1; index++)
	{
		if(hashMap[index].limit)
		{
			free(hashMap[index].indices);
			free(hashMap[index].vertices);
		}
	}

	free(hashMap);
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
	printlog(1, "Create Vertex Buffer: Size = %lu bytes", vertexCount * vertexSize);

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
	printlog(1, "Create Index Buffer: Size = %lu bytes", indexCount * indexSize);

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

	printlog(1, "Create Uniform Buffers");
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
	 "Create Descriptor Pool");
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
	 "Allocate Descriptor Sets");
	free(layouts);

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

	printlog(1, "Update Descriptor Sets");
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
	 "Allocate Command Buffers");

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

		printlog(vkBeginCommandBuffer(commandBuffers[commandIndex], &beginInfo) == VK_SUCCESS, NULL);
		vkCmdBeginRenderPass(commandBuffers[commandIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffers[commandIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
		vkCmdBindDescriptorSets(commandBuffers[commandIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
		 pipelineLayout, 0, 1, &descriptorSets[commandIndex], 0, NULL);
		vkCmdBindVertexBuffers(commandBuffers[commandIndex], 0, 1, &vertexBuffer, (VkDeviceSize[]){0});
		vkCmdBindIndexBuffer(commandBuffers[commandIndex], indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffers[commandIndex], indexCount, 1, 0, 0, 0);
		vkCmdEndRenderPass(commandBuffers[commandIndex]);
		printlog(vkEndCommandBuffer(commandBuffers[commandIndex]) == VK_SUCCESS, NULL);
	}

	printlog(1, "Record Commands");
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

	printlog(1, "Create Syncronization Objects");
}

void recreatePipeline()
{
	vkDeviceWaitIdle(device);
	cleanupPipeline();

	createGraphicsPipeline();
	createCommandBuffers();
}

void recreateSwapchain()
{
	while(width == 0 || height == 0)
	{
		glfwWaitEvents();
		glfwGetFramebufferSize(window, &width, &height);
	}
	vkDeviceWaitIdle(device);

	cleanupSwapchain();
	swapchainDetails = generateSwapchainDetails(physicalDevice);

	createSwapchain();
	createRenderPass();
	createGraphicsPipeline();
	createColorBuffer();
	createDepthBuffer();
	createFramebuffers();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createCommandBuffers();
}

void setup()
{
	glfwInit();
	createInstance();
	registerMessenger();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapchain();
	createRenderPass();
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createCommandPool();
	createColorBuffer();
	createDepthBuffer();
	createFramebuffers();
	createTextureImage();
	createTextureSampler();
	createObjectModels();
	createVertexBuffer();
	createIndexBuffer();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createCommandBuffers();
	createSyncObjects();
}

void directionVector(float v[], float t[])
{
	v[0] = t[0];
	v[1] = t[1];
	v[2] = t[2];
	v[3] = 0.0f;
}

void positionVector(float v[], float t[])
{
	v[0] = t[0];
	v[1] = t[1];
	v[2] = t[2];
	v[3] = 1.0f;
}

void identityMatrix(float m[])
{
	m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void normalize(float v[])
{
	const float eps = 0.0009765625f;
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

void multiplyVector(float a[], float b[], float c[])
{
	for(int row = 0; row < 4; row++)
		for(int itr = 0; itr < 4; itr++)
			c[row] += a[4 * row + itr] * b[itr];
}

void multiplyMatrix(float a[], float b[], float c[])
{
	for(int row = 0; row < 4; row++)
		for(int col = 0; col < 4; col++)
			for(int itr = 0; itr < 4; itr++)
				c[4 * row + col] += a[4 * row + itr] * b[4 * itr + col];
}

void scaleVector(float v[], float t)
{
	v[0] *= t;
	v[1] *= t;
	v[2] *= t;
}

void scaleMatrix(float m[], float v[])
{
	float k[] = {
		v[0], 0.0f, 0.0f, 0.0f,
		0.0f, v[1], 0.0f, 0.0f,
		0.0f, 0.0f, v[2], 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}, t[16] = {0};

	multiplyMatrix(k, m, t);
	memcpy(m, t, sizeof(t));
}

void translateVector(float v[], float t[])
{
	v[0] += t[0] * v[3];
	v[1] += t[1] * v[3];
	v[2] += t[2] * v[3];
}

void translateMatrix(float m[], float v[])
{
	float k[] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		v[0], v[1], v[2], 1.0f
	}, t[16] = {0};

 	multiplyMatrix(k, m, t);
	memcpy(m, t, sizeof(t));
}

void rotate(float f[], float w[], float r, int m)
{
	normalize(w);

	float x = w[0], y = w[1], z = w[2], s = sinf(r), c = cosf(r), d = 1 - cosf(r);
	float xx = x * x * d, xy = x * y * d, xz = x * z * d, yy = y * y * d, yz = y * z * d, zz = z * z * d;

	float k[] = {
		xx + c,     xy + s * z, xz - s * y, 0.0f,
		xy - s * z, yy + c,     yz + s * x, 0.0f,
		xz + s * y, yz - s * x, zz + c,     0.0f,
		0.0f,       0.0f,       0.0f,       1.0f
	};

	if(!m)
	{
		float t[4] = {0};
		multiplyVector(k, f, t);
		memcpy(f, t, sizeof(t));
	}

	else
	{
		float t[16] = {0};
		multiplyMatrix(k, f, t);
		memcpy(f, t, sizeof(t));
	}
}

void rotateVector(float v[], float w[], float r)
{
	rotate(v, w, r, 0);
}

void rotateMatrix(float m[], float w[], float r)
{
	rotate(m, w, r, 1);
}

void cameraMatrix(float m[], float eye[], float cent[], float top[])
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

void orthographicMatrix(float m[], float h, float r, float n, float f)
{
	float k[] = {
		2 / (h * r),  0.0f,         0.0f,         0.0f,
		0.0f,        -2 / h,        0.0f,         0.0f,
		0.0f,         0.0f,         1 / (n - f),  0.0f,
		0.0f,         0.0f,         n / (n - f),  1.0f
	};

	memcpy(m, k, sizeof(k));
}

void frustumMatrix(float m[], float h, float r, float n, float f)
{
	float k[] = {
		2 * n / (h * r),  0.0f,             0.0f,             0.0f,
		0.0f,            -2 * n / h,        0.0f,             0.0f,
		0.0f,             0.0f,             f / (n - f),     -1.0f,
		0.0f,             0.0f,             n * f / (n - f),  0.0f
	};

	memcpy(m, k, sizeof(k));
}

void perspectiveMatrix(float m[], float fov, float asp, float n, float f)
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
	UniformBufferObject ubo = {0};
	float center[4], left[4], direction[4];

	if(!ready)
	{
		timeorig = timespec;
		directionVector(up, (float[]){0.0f, 0.0f, -1.0f});
		directionVector(forward, (float[]){1.0f, 0.0f, 0.0f});
		positionVector(position, (float[]){-3.2f, 0.0f, -0.4f});
		normalize(forward);
		normalize(up);
		ready = 1;
	}

	long timediff = 1e6L * (timespec.tv_sec - timeorig.tv_sec) + (timespec.tv_nsec - timeorig.tv_nsec) / 1e3L;
	float delta = M_PI * timediff / (4e6L * sqrtf(fmaxf(1, (keyW || keyS) + (keyA || keyD) + (keyR || keyF))));
	timeorig = timespec;

	cross(up, forward, left);
	rotateVector(forward, up, M_PI * moveX / width);
	rotateVector(forward, left, -M_PI * moveY / height);
	moveX = moveY = 0;

	directionVector(direction, forward);
	scaleVector(direction, delta * (keyW - keyS));
	translateVector(position, direction);

	directionVector(direction, left);
	scaleVector(direction, delta * (keyA - keyD));
	translateVector(position, direction);

	directionVector(direction, up);
	scaleVector(direction, delta * (keyR - keyF));
	translateVector(position, direction);

	position[2] = fminf(-0.4f, position[2]);
	positionVector(center, position);
	translateVector(center, forward);

	identityMatrix(ubo.model);
	cameraMatrix(ubo.view, position, center, up);
	perspectiveMatrix(ubo.proj, M_PI / 2, (float)width / (float)height, 0.01f, 100.0f);

	void *data;
	vkMapMemory(device, uniformBufferMemories[index], 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(device, uniformBufferMemories[index]);
}

void draw()
{
	time_t currentTime = 0;
	uint32_t currentFrame = 0, frameCount = 0, checkPoint = 0;
	printlog(1, "Begin Drawing");

	while(!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
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
			glfwSetWindowTitle(window, title);
			currentTime = timespec.tv_sec;
			checkPoint = frameCount;
		}
	}

	printlog(1, "Finish Drawing");
	vkDeviceWaitIdle(device);
}

void cleanupPipeline()
{
	vkFreeCommandBuffers(device, commandPool, framebufferSize, commandBuffers);
	vkDestroyPipeline(device, graphicsPipeline, NULL);
	vkDestroyPipelineLayout(device, pipelineLayout, NULL);

	free(commandBuffers);
}

void cleanupSwapchain()
{
	vkDestroyImageView(device, depthView, NULL);
	vkDestroyImage(device, depthImage, NULL);
	vkFreeMemory(device, depthMemory, NULL);
	vkDestroyImageView(device, colorView, NULL);
	vkDestroyImage(device, colorImage, NULL);
	vkFreeMemory(device, colorMemory, NULL);
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
	free(swapchainViews);
	free(swapchainImages);
	free(swapchainFramebuffers);
	free(uniformBuffers);
	free(uniformBufferMemories);
	free(descriptorSets);
	free(commandBuffers);
}

void clean()
{
	printlog(1, "Start Cleaning");
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
	vkDestroyImageView(device, colorView, NULL);
	vkDestroyImage(device, colorImage, NULL);
	vkFreeMemory(device, colorMemory, NULL);
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
	glfwDestroyWindow(window);
	PFN_vkDestroyDebugUtilsMessengerEXT destroyMessenger =
	 (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if(destroyMessenger != NULL)
		destroyMessenger(instance, messenger, NULL);
	vkDestroyInstance(instance, NULL);
	glfwTerminate();
	printlog(1, "End Cleaning");
}

int main()
{
	setup();
	draw();
	clean();
}
