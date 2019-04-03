#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <xcb/xcb.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>
#include <blis/blis.h>
#include "libraries/stb_image.h"
#include "libraries/tinyobj_loader_c.h"
//#include "libraries/device_support.h"

# define PI 3.14159265358979f

struct vertex
{
	float pos[2];
	float col[3];
};

struct uniformBufferObject
{
    float model[4];
    float view[4];
    float proj[4];
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
uint16_t *indices;
VkBuffer vertexBuffer, indexBuffer;
VkBuffer *uniformBuffers;
VkDeviceMemory vertexBufferMemory, indexBufferMemory;
VkDeviceMemory *uniformBufferMemories;
VkCommandPool commandPool;
VkCommandBuffer *commandBuffers;
VkSemaphore *imageAvailable, *renderFinished;
VkFence *frameFences;

void setup();
void clean();
void recreateSwapchain();
void cleanupSwapchain();

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
	
	if(vkCreateInstance(&instanceInfo, NULL, &instance) == VK_SUCCESS)
		printf("Created Instance: LunarG\n");
}

static VKAPI_ATTR VkBool32 VKAPI_CALL messageCallback(
 VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	(void)type;
	(void)pUserData;
		
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

void createWindow()
{
	width = 240;
	height = 240;
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
	xcb_change_property(xconn, XCB_PROP_MODE_REPLACE, window, protReply->atom, XCB_ATOM_ATOM,
	 32, 1, &reply->atom);
	destroyEvent = reply->atom;
	
	const char *title = "Vulkan";
	xcb_change_property(xconn, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
	 8, strlen(title), title);
	
	xcb_map_window(xconn, window);
	xcb_flush(xconn);
	printf("Created Window: Xorg XCB\n");
}

void createSurface()
{
	VkXcbSurfaceCreateInfoKHR surfaceInfo = {0};
	surfaceInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	surfaceInfo.connection = xconn;
	surfaceInfo.window = window;
	
	if(vkCreateXcbSurfaceKHR(instance, &surfaceInfo, NULL, &surface) == VK_SUCCESS)
		printf("Created Surface: XCB Surface\n");
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
		
		if(formatCount && modeCount && swapchainSupport && deviceFeatures.geometryShader)
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
	
	if(maxScore != -1)
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
	
	if(vkCreateDevice(physicalDevice, &deviceInfo, NULL, &device) == VK_SUCCESS)
		printf("Created Logical Device: Queue Count = %u\n", deviceInfo.queueCreateInfoCount);
	vkGetDeviceQueue(device, graphicsIndex, 0, &graphicsQueue);
	printf("Acquired Graphics Queue Handle: Index = %u\n", graphicsIndex);
	vkGetDeviceQueue(device, presentIndex, 0, &presentQueue);
	printf("Acquired Presentation Queue Handle: Index = %u\n", presentIndex);
	free(queueList);
}

VkSurfaceFormatKHR chooseSurfaceFormat(VkSurfaceFormatKHR* surfaceFormats, uint32_t formatCount)
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

VkPresentModeKHR choosePresentationMode(VkPresentModeKHR* presentModes, uint32_t modeCount)
{
	int8_t mailbox = 0, relaxed = 0;
	for(uint32_t modeIndex = 0; modeIndex < modeCount; modeIndex++)
	{
		if(presentModes[modeIndex] == VK_PRESENT_MODE_MAILBOX_KHR)
			mailbox = 1;
		else if(presentModes[modeIndex] == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
			relaxed = 1;
	}
	
	if(mailbox)
		return VK_PRESENT_MODE_MAILBOX_KHR;
	else if(relaxed)
		return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	else
		return VK_PRESENT_MODE_FIFO_KHR;
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
		uint32_t queueIndices[] = {graphicsIndex, presentIndex};
		swapchainInfo.queueFamilyIndexCount = 2;
		swapchainInfo.pQueueFamilyIndices = queueIndices;
		swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
	}
	
	if(vkCreateSwapchainKHR(device, &swapchainInfo, NULL, &swapchain) == VK_SUCCESS)
		printf("Created Swapchain: %s\n", presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "Mailbox" : "FIFO");
	vkGetSwapchainImagesKHR(device, swapchain, &framebufferSize, NULL);
	swapchainImages = malloc(framebufferSize * sizeof(VkImage));
	vkGetSwapchainImagesKHR(device, swapchain, &framebufferSize, swapchainImages);
	if(framebufferSize && swapchainImages)
		printf("Acquired Swapchain Images: Count = %u\n", framebufferSize);
}

void createImageViews()
{
	swapchainViews = malloc(framebufferSize * sizeof(VkImageView));
	for(uint32_t viewIndex = 0; viewIndex < framebufferSize; viewIndex++)
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
		if(vkCreateImageView(device, &viewInfo, NULL, &swapchainViews[viewIndex]) == VK_SUCCESS)
			printf("Acquired Image View: Number = %u\n", viewIndex);
	}
}

void createRenderPass()
{
	VkAttachmentReference colorAttachmentReference = {0};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	
	VkSubpassDescription subpass = {0};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;
	
	VkAttachmentDescription colorAttachmentDescription = {0};
	colorAttachmentDescription.format = swapchainFormat;
	colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	
	VkSubpassDependency dependency = {0};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	
	VkRenderPassCreateInfo renderPassInfo = {0};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachmentDescription;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;
	
	if(vkCreateRenderPass(device, &renderPassInfo, NULL, &renderPass) == VK_SUCCESS)
		printf("Created Render Pass: Subpass Count = %u\n", renderPassInfo.subpassCount);
}

void createDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding uniformBufferBinding = {0};
	uniformBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniformBufferBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uniformBufferBinding.descriptorCount = 1;
	uniformBufferBinding.binding = 0;
	
	VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &uniformBufferBinding;
	
	if(vkCreateDescriptorSetLayout(device, &layoutInfo, NULL, &descriptorSetLayout) == VK_SUCCESS)
		printf("Created Descriptor Set Layout: Binding Count = %d\n", layoutInfo.bindingCount);
}

VkShaderModule initializeShaderModule(const char* shaderName, const char* filePath)
{
	FILE *file = fopen(filePath, "rb");
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	rewind(file);
	
	uint32_t *shaderData = calloc(size, 1);
	if(fread(shaderData, 1, size, file) != size)
		return NULL; //TODO: implement error handling
	fclose(file);
	
	VkShaderModuleCreateInfo shaderInfo = {0};
	shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderInfo.codeSize = size;
	shaderInfo.pCode = shaderData;
	
	VkShaderModule shaderModule;
	if(vkCreateShaderModule(device, &shaderInfo, NULL, &shaderModule) == VK_SUCCESS)
		printf("Created %s Shader Module: %zd bytes\n", shaderName, size);
	free(shaderData);
	return shaderModule;
}

VkVertexInputBindingDescription generateVertexInputBinding()
{
	VkVertexInputBindingDescription inputBinding = {0};
	inputBinding.binding = 0;
	inputBinding.stride = sizeof(Vertex);
	inputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return inputBinding;
}

VkVertexInputAttributeDescription generatePositionInputAttributes()
{
	VkVertexInputAttributeDescription inputAttribute = {0};
	inputAttribute.binding = 0;
	inputAttribute.location = 0;
	inputAttribute.format = VK_FORMAT_R32G32_SFLOAT;
	inputAttribute.offset = 0;
	return inputAttribute;
}

VkVertexInputAttributeDescription generateColorInputAttributes()
{
	VkVertexInputAttributeDescription inputAttribute = {0};
	inputAttribute.binding = 0;
	inputAttribute.location = 1;
	inputAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	inputAttribute.offset = sizeof(float) * 2;
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
	VkVertexInputAttributeDescription inputAttributes[] = 
	 {generatePositionInputAttributes(), generateColorInputAttributes()};
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {0};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = 2;
	vertexInputInfo.pVertexBindingDescriptions = &inputBinding;
	vertexInputInfo.pVertexAttributeDescriptions = inputAttributes;
	
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
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
	 VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
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
	
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
	
	if(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &pipelineLayout) == VK_SUCCESS)
		printf("Created Pipeline Layout: Set Layout Count = %d\n", pipelineLayoutInfo.setLayoutCount);
	
	VkPipelineShaderStageCreateInfo shaderStages[] = {vertexStageInfo, fragmentStageInfo};
	VkGraphicsPipelineCreateInfo pipelineInfo = {0};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
	pipelineInfo.pViewportState = &viewportStateInfo;
	pipelineInfo.pRasterizationState = &rasterizerInfo;
	pipelineInfo.pMultisampleState = &multisamplingInfo;
	pipelineInfo.pColorBlendState = &colorBlendInfo;
	pipelineInfo.layout = pipelineLayout;
	
	if(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &graphicsPipeline) == VK_SUCCESS)
		printf("Created Graphics Pipeline: %u x %u\n", width, height);
	
	vkDestroyShaderModule(device, fragmentShader, NULL);
	vkDestroyShaderModule(device, vertexShader, NULL);
}

void createFramebuffers()
{
	swapchainFramebuffers = malloc(framebufferSize * sizeof(VkFramebuffer));
	
	for(uint32_t framebufferIndex = 0; framebufferIndex < framebufferSize; framebufferIndex++)
	{
		VkFramebufferCreateInfo framebufferInfo = {0};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.layers = 1;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &swapchainViews[framebufferIndex];
		framebufferInfo.width = swapchainExtent.width;
		framebufferInfo.height = swapchainExtent.height;
		
		if(vkCreateFramebuffer(device, &framebufferInfo, NULL,
		 &swapchainFramebuffers[framebufferIndex]) == VK_SUCCESS)
			printf("Created Framebuffer: Number = %u\n", framebufferIndex);
	}
}

void createCommandPool()
{
	VkCommandPoolCreateInfo poolInfo = {0};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = graphicsIndex;
	
	if(vkCreateCommandPool(device, &poolInfo, NULL, &commandPool) == VK_SUCCESS)
		printf("Created Command Pool: Queue Index = %u\n", graphicsIndex);
}

uint32_t chooseMemoryType(uint32_t filter, VkMemoryPropertyFlags flags)
{
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	for(uint32_t index = 0; index < memoryProperties.memoryTypeCount; index++)
		if((filter & (1 << index)) &&
		 (memoryProperties.memoryTypes[index].propertyFlags & flags) == flags)
			return index;
	
	return 0;
}

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
 VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory)
{
	VkBufferCreateInfo bufferInfo = {0};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferInfo.usage = usage;
	bufferInfo.size = size;

	if(vkCreateBuffer(device, &bufferInfo, NULL, buffer) == VK_SUCCESS)
		printf("Created Buffer: Size = %lu\n", bufferInfo.size);

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, *buffer, &memoryRequirements);

	VkMemoryAllocateInfo allocateInfo = {0};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex = chooseMemoryType(memoryRequirements.memoryTypeBits, properties);

	if(vkAllocateMemory(device, &allocateInfo, NULL, bufferMemory) == VK_SUCCESS)
		printf("Allocated Buffer Memory: Size = %lu\n", memoryRequirements.size);
	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}

void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBufferAllocateInfo allocateInfo = {0};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandPool = commandPool;
	allocateInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	if(vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer) == VK_SUCCESS)
		printf("Allocated Command Buffer: Copy Queue\n");

	VkCommandBufferBeginInfo beginInfo = {0};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	
	VkBufferCopy copyRegion = {0};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	
	vkBeginCommandBuffer(commandBuffer, &beginInfo);
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {0};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);
	printf("Successfuly Copied Buffer: Size = %lu\n", size);

	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void createVertexBuffer()
{
	/*Vertex buffer[] = {
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
		{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
	};*/

	Vertex buffer[] = {
		{{0.0f, 0.8f}, {0.8f, 0.4f, 0.0f}},
		{{-0.7f, -0.2f}, {0.8f, 0.4f, 0.0f}},
		{{0.7f, -0.2f}, {0.8f, 0.4f, 0.0f}},
		{{-0.9f, 0.0f}, {0.8f, 0.4f, 0.0f}},
		{{-0.7f, -0.8f}, {0.8f, 0.4f, 0.0f}},
		{{0.7f, -0.8f}, {0.8f, 0.4f, 0.0f}},
		{{0.9f, 0.0f}, {0.8f, 0.4f, 0.0f}}
	};

	vertices = buffer;
	vertexSize = sizeof(Vertex);
	vertexCount = sizeof(buffer) / vertexSize;

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
	printf("Copied Vertex Buffer Into Memory: Size = %lu\n", vertexCount * vertexSize);

	vkFreeMemory(device, stagingBufferMemory, NULL);
	vkDestroyBuffer(device, stagingBuffer, NULL);
}

void createIndexBuffer()
{
	//uint16_t buffer[] = {0, 1, 2, 2, 3, 0};
	uint16_t buffer[] = {0, 1, 2, 0, 3, 4, 0, 5, 6};

	indices = buffer;
	indexSize = sizeof(uint16_t);
	indexCount = sizeof(buffer) / indexSize;

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
	printf("Copied Index Buffer Into Memory: Size = %lu\n", indexCount * indexSize);

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
	printf("Created Uniform Buffers: Count = %d\n", framebufferSize);
}

void createCommandBuffers()
{
	commandBuffers = malloc(framebufferSize * sizeof(VkCommandBuffer));
	VkCommandBufferAllocateInfo allocateInfo = {0};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = framebufferSize;
	
	if(vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers) == VK_SUCCESS)
		printf("Allocated Command Buffers: Size = %u\n", framebufferSize);
	
	for(uint32_t commandIndex = 0; commandIndex < framebufferSize; commandIndex++)
	{
		VkCommandBufferBeginInfo beginInfo = {0};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		
		if(vkBeginCommandBuffer(commandBuffers[commandIndex], &beginInfo) == VK_SUCCESS)
			printf("Started Command Buffer Recording: Index = %u\n", commandIndex);
		
		VkOffset2D offset = {0};
		VkClearValue backgroundColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
		VkRenderPassBeginInfo renderPassBeginInfo = {0};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = swapchainFramebuffers[commandIndex];
		renderPassBeginInfo.renderArea.offset = offset;
		renderPassBeginInfo.renderArea.extent = swapchainExtent;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &backgroundColor;
		vkCmdBeginRenderPass(commandBuffers[commandIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffers[commandIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		VkDeviceSize offsets[] = {0};
		VkBuffer vertexBuffers[] = {vertexBuffer};
		vkCmdBindVertexBuffers(commandBuffers[commandIndex], 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffers[commandIndex], indexBuffer, 0, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(commandBuffers[commandIndex], indexCount, 1, 0, 0, 0);
		
		vkCmdEndRenderPass(commandBuffers[commandIndex]);
		if(vkEndCommandBuffer(commandBuffers[commandIndex]) == VK_SUCCESS)
			printf("Successfuly Completed Recording: Index = %u\n", commandIndex);
	}
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
	
	printf("Created Syncronization Objects: Count = %u\n", framebufferLimit);
}

void recreateSwapchain()
{
	vkDeviceWaitIdle(device);
	cleanupSwapchain();
	swapchainDetails =
	 generateSwapchainDetails(physicalDevice);
	
	createSwapchain();
	createImageViews();
	createRenderPass();
	createGraphicsPipeline();
	createFramebuffers();
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
	createImageViews();
	createRenderPass();
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createFramebuffers();
	createCommandPool();
	createVertexBuffer();
	createIndexBuffer();
	createUniformBuffers();
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

void scale(float* M, float x, float y, float z)
{
	float K[] = {
		x,    0.0f, 0.0f, 0.0f,
		0.0f, y,    0.0f, 0.0f,
		0.0f, 0.0f, z,    0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}, T[16];

	bli_sgemm(BLIS_NO_TRANSPOSE, BLIS_NO_TRANSPOSE, 4, 4, 4,
	 &(float){1.0f}, K, 4, 1, M, 4, 1, &(float){0.0f}, T, 4, 1);
	memcpy(M, T, sizeof(T));
	bli_sprintm("", 4, 4, M, 4, 1, "%.2f", "");
}

void translate(float* M, float x, float y, float z)
{
	float K[] = {
		1.0f, 0.0f, 0.0f, x,
		0.0f, 1.0f, 0.0f, y,
		0.0f, 0.0f, 1.0f, z,
		0.0f, 0.0f, 0.0f, 1.0f
	}, T[16];

	bli_sgemm(BLIS_NO_TRANSPOSE, BLIS_NO_TRANSPOSE, 4, 4, 4,
	 &(float){1.0f}, K, 4, 1, M, 4, 1, &(float){0.0f}, T, 4, 1);
	memcpy(M, T, sizeof(T));
	bli_sprintm("", 4, 4, M, 4, 1, "%.2f", "");
}

void rotate(float* M, float x, float y, float z, float th)
{
	float mag = sqrtf(x * x + y * y + z * z), eps = 0.0009765625f; //Epsilon = 2 ^ -10
	if(abs(1.0f - mag) > eps)
	{
		x /= mag;
		y /= mag;
		z /= mag;
	}

	float xx = x * x, xy = x * y, xz = x * z, yy = y * y, yz = y * z, zz = z * z;
	float st = sinf(th), ct = cosf(th), rct = 1 - cosf(th);

	float K[] = {
		ct + xx * rct,     xy * rct - z * st, xz * rct + y * st, 0,
		xy * rct + z * st, ct + yy * rct,     yz * rct - x * st, 0,
		xz * rct - y * st, yz * rct + x * st, ct + zz * rct,     0,
		0,                 0,                 0,                 1
	}, T[16];

	bli_sgemm(BLIS_NO_TRANSPOSE, BLIS_NO_TRANSPOSE, 4, 4, 4,
	 &(float){1.0f}, K, 4, 1, M, 4, 1, &(float){0.0f}, T, 4, 1);
	memcpy(M, T, sizeof(T));
	bli_sprintm("", 4, 4, M, 4, 1, "%.2f", "");
}

void updateUniformBuffer(int index)
{
	//TODO: implement rotation
}

void draw()
{
	uint32_t currentFrame = 0, frameCount = 0;

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
		
		updateUniformBuffer(imageIndex);
		
		VkSemaphore waitSemaphores[] = {imageAvailable[currentFrame]};
		VkSemaphore signalSemaphores[] = {renderFinished[currentFrame]};
		VkPipelineStageFlags stageFlags[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		
		VkSubmitInfo submitInfo = {0};
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
		
		VkSwapchainKHR swapchains[] = {swapchain};
		VkPresentInfoKHR presentInfo = {0};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &imageIndex;
		
		VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
		if(presentResult == VK_SUBOPTIMAL_KHR || presentResult == VK_ERROR_OUT_OF_DATE_KHR)
			recreateSwapchain();
		
		currentFrame = ++frameCount % framebufferLimit;
	}
	
	free(event);
	vkDeviceWaitIdle(device);
}

void cleanupSwapchain()
{
	for(uint32_t framebufferIndex = 0; framebufferIndex < framebufferSize; framebufferIndex++)
		vkDestroyFramebuffer(device, swapchainFramebuffers[framebufferIndex], NULL);
	vkFreeCommandBuffers(device, commandPool, framebufferSize, commandBuffers);
	vkDestroyPipeline(device, graphicsPipeline, NULL);
	vkDestroyPipelineLayout(device, pipelineLayout, NULL);
	vkDestroyRenderPass(device, renderPass, NULL);
	for(uint32_t viewIndex = 0; viewIndex < framebufferSize; viewIndex++)
		vkDestroyImageView(device, swapchainViews[viewIndex], NULL);
	vkDestroySwapchainKHR(device, swapchain, NULL);
	free(swapchainDetails.presentModes);
	free(swapchainDetails.surfaceFormats);
}

void clean()
{
	for(uint32_t syncIndex = 0; syncIndex < framebufferLimit; syncIndex++)
	{
		vkDestroyFence(device, frameFences[syncIndex], NULL);
		vkDestroySemaphore(device, renderFinished[syncIndex], NULL);
		vkDestroySemaphore(device, imageAvailable[syncIndex], NULL);
	}
	vkDestroyCommandPool(device, commandPool, NULL);
	for(size_t uniformIndex = 0; uniformIndex < framebufferSize; uniformIndex++)
	{
		vkDestroyBuffer(device, uniformBuffers[uniformIndex], NULL);
		vkFreeMemory(device, uniformBufferMemories[uniformIndex], NULL);
	}
	vkDestroyBuffer(device, indexBuffer, NULL);
	vkFreeMemory(device, indexBufferMemory, NULL);
	vkDestroyBuffer(device, vertexBuffer, NULL);
	vkFreeMemory(device, vertexBufferMemory, NULL);
	for(uint32_t framebufferIndex = 0; framebufferIndex < framebufferSize; framebufferIndex++)
		vkDestroyFramebuffer(device, swapchainFramebuffers[framebufferIndex], NULL);
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
}

int main()
{
	setup();
	draw();
	clean();
}
