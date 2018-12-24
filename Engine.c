#include <stdio.h>
#include <vulkan/vulkan.h>

#ifdef _WIN32
	#include <Windows.h>
	#include <vulkan/vulkan_win32.h>
#elif __linux__
	#include <xcb/xcb.h>
	#include <vulkan/vulkan_xcb.h>
#endif

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

void setup()
{
#ifdef _WIN32
	HINSTANCE instanceHandle = GetModuleHandle(0);
	printf("%p\n", instanceHandle);
	WNDCLASS windowClass = { CS_DBLCLKS, WindowProc, 0, 0, instanceHandle,
		LoadIcon(0,IDI_APPLICATION), LoadCursor(0,IDC_ARROW),
		CreateSolidBrush(COLOR_BACKGROUND), 0, "Engine" };
	RegisterClass(&windowClass);
	HWND windowHandle = CreateWindow("Engine", "Vulkan", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		0, 0, instanceHandle, 0);
	ShowWindow(windowHandle, SW_SHOWDEFAULT);
	UpdateWindow(windowHandle);
#endif
}

void draw()
{
#ifdef _WIN32
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		DispatchMessage(&msg);
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
		printf("%d\n", extensionCount);
	}
	return msg.wParam;
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
