#include <windows.h>
#define IDC_BUTTON 101
#pragma comment(linker,"\"/manifestdependency:type='win32' "\
	"name='Microsoft.Windows.Common-Controls' "\
	"version='6.0.0.0' processorArchitecture='*' "\
	"publicKeyToken='6595b64144ccf1df' language='*'\"")


BOOL InitApplication(HINSTANCE);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ButtonProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
//int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);

static TCHAR szAppName[] = TEXT("MainWClass");
WNDPROC BTNProc;
HWND hwndButton;

int WINAPI WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow)
{
	MSG msg;

	if (!InitApplication(hInstance))
	{
		return (FALSE);
	}

	if (!InitInstance(hInstance, nCmdShow))
	{
		return (FALSE);
	}

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (msg.wParam);

}

BOOL InitApplication(HINSTANCE hinstance)
{

	WNDCLASS wcx;
	//wcx.cbSize = sizeof(wcx);
	wcx.style = CS_HREDRAW | CS_VREDRAW;
	wcx.lpfnWndProc = (WNDPROC)WndProc;
	wcx.cbClsExtra = 0;
	wcx.cbWndExtra = 0;
	wcx.hInstance = hinstance;
	wcx.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcx.hbrBackground = NULL;
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = szAppName;

	return (RegisterClass(&wcx));
}

BOOL InitInstance(HINSTANCE hinstance, int nCmdShow)
{
	HWND hwnd;
	HINSTANCE hinst = hinstance;

	hwnd = CreateWindow(szAppName,
		TEXT("SDK"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		hinstance,
		NULL);
	if (!hwnd)
	{
		return (FALSE);
	}

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);
	return (TRUE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;

	RECT rect;
	PAINTSTRUCT ps;
	//RECT rect;

	switch (uMsg)
	{
	case WM_CREATE:
		hwndButton = CreateWindow(TEXT("BUTTON"),
			TEXT("OK"),
			WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
			10,
			10,
			100,
			27,
			hwnd,
			(HMENU)IDC_BUTTON,
			(HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE),
			NULL);

	
		return 0;
	case WM_PAINT:
		hdc = BeginPaint(hwnd, &ps);
		EndPaint(hwnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
LRESULT CALLBACK ButtonProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	RECT rect;
	switch (uMsg)
	{
	case WM_PAINT:
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_BUTTON:
			hdc = GetDC(hwnd);
			TextOut(hdc, 100, 100, TEXT("Mouse Left Button Click!"), strlen("Mouse Left Button Click!"));
			ReleaseDC(hwnd, hdc);
			ValidateRect(hwnd, &rect);
			break;
		}
		break;
	}
	return CallWindowProc(BTNProc, hwnd, uMsg, wParam, lParam);
}