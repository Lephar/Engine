#include <Windows.h>
#include <vector>
#include <string>
#include <iostream>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

using namespace std;

VkInstance instance;
VkDebugUtilsMessengerEXT messenger;
uint32_t presentIndex, graphicsIndex;
VkPhysicalDevice physicalDevice;
VkQueue presentQueue, graphicsQueue;
VkDevice device;
VkSurfaceKHR surface;

LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);

void createInstance()
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Tutorial";
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

	std::vector<string> extensionNames{VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
	std::vector<string> layerNames{"VK_LAYER_LUNARG_standard_validation"};
	
	VkInstanceCreateInfo instanceInfo = {};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.enabledExtensionCount = extensionNames.size();
	instanceInfo.ppEnabledExtensionNames = extensionNames.data();
	instanceInfo.enabledLayerCount = layerNames.size();
	instanceInfo.ppEnabledLayerNames = layerNames.data();

	if (vkCreateInstance(&instanceInfo, nullptr, &instance) == VK_SUCCESS)
		printf("Created Instance! (LunarG)\n");
	else
		printf("anan\n");
}

int wmain(int argc, wchar_t *argv[], wchar_t *envp[])
{
	HINSTANCE instanceHandle = GetModuleHandle(0);
	WNDCLASS windowClass = {CS_DBLCLKS, WindowProc, 0, 0, instanceHandle,
		LoadIcon(0,IDI_APPLICATION), LoadCursor(0,IDC_ARROW),
		HBRUSH(COLOR_WINDOW + 1), 0, L"VulkanWin32"};
	RegisterClass(&windowClass);
	
	HWND windowHandle = CreateWindow(L"VulkanWin32", L"Vulkan", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		0, 0, instanceHandle, 0);
	ShowWindow(windowHandle, SW_SHOWDEFAULT);
	UpdateWindow(windowHandle);

	createInstance();

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
		DispatchMessage(&msg);
	return msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if(message == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}