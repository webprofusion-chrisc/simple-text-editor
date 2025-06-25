#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <fstream>
#include <string>
#include <vector>
#include <richedit.h>
#include "resource.h"

#define IDC_MAIN_EDIT 101
#define IDC_TAB_CTRL 200
#define IDM_FILE_OPEN 102
#define IDM_FILE_SAVEAS 103
#define IDM_FILE_EXIT 104
#define IDM_FILE_SAVE 105

// Link with comctl32.lib for InitCommonControlsEx
#pragma comment(lib, "comctl32.lib")

struct EditorTab {
	HWND hwndEdit;
	wchar_t filePath[MAX_PATH];
};

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void DoFileOpen(HWND hwndTab, HWND hwnd);
void DoFileSaveAs(HWND hwndTab, HWND hwnd);
void OpenFileToEditor(HWND hwndEdit, const wchar_t* szFile);
void DoFileSave(HWND hwndTab, HWND hwnd);
void AddTab(HWND hwndTab, const wchar_t* filePath, HWND hwndEdit);
void SwitchToTab(HWND hwndTab, int index);
int GetCurrentTabIndex(HWND hwndTab);
EditorTab* GetTabData(HWND hwndTab, int index);

// Global font handle
HFONT g_hFont = NULL;
std::vector<EditorTab> g_tabs;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow)
{
	SetProcessDPIAware();

	LoadLibraryW(L"Msftedit.dll");

	INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_TAB_CLASSES };
	InitCommonControlsEx(&icex);

	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"SimpleTextEditor";
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APPICON));

	RegisterClass(&wc);

	HWND hwnd = CreateWindow(wc.lpszClassName, L"Simple Text Editor", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, NULL, NULL, hInstance, NULL);

	DragAcceptFiles(hwnd, TRUE);

	// Support opening a file from the command line (Open With)
	int argc = 0;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv && argc > 1)
	{
		// argv[1] is the file path
		const wchar_t* szFile = argv[1];
		HWND hwndTab = FindWindowEx(hwnd, NULL, WC_TABCONTROL, NULL);
		if (hwndTab)
		{
			HWND hwndEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"RICHEDIT50W", L"",
				WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL,
				0, 30, 800, 600, hwnd, NULL, GetModuleHandle(NULL), NULL);
			SendMessage(hwndEdit, EM_EXLIMITTEXT, 0, -1);
			RECT rcClient;
			GetClientRect(hwnd, &rcClient);
			MoveWindow(hwndEdit, 0, 30, rcClient.right, rcClient.bottom - 30, TRUE);
			if (!g_hFont)
			{
				g_hFont = CreateFont(
					-MulDiv(11, GetDeviceCaps(GetDC(hwnd), LOGPIXELSY), 72),
					0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
					OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
					FIXED_PITCH | FF_MODERN, L"Consolas");
			}
			SendMessage(hwndEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
			OpenFileToEditor(hwndEdit, szFile);
			AddTab(hwndTab, szFile, hwndEdit);
		}
		LocalFree(argv);
	}

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);

		DispatchMessage(&msg);
	}

	return 0;
}

void AddMenus(HWND hwnd)
{
	HMENU hMenubar = CreateMenu();
	HMENU hMenu = CreateMenu();

	AppendMenu(hMenu, MF_STRING, IDM_FILE_OPEN, L"Open");

	AppendMenu(hMenu, MF_STRING, IDM_FILE_SAVE, L"Save");

	AppendMenu(hMenu, MF_STRING, IDM_FILE_SAVEAS, L"Save As");

	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

	AppendMenu(hMenu, MF_STRING, IDM_FILE_EXIT, L"Exit");

	AppendMenu(hMenubar, MF_POPUP, (UINT_PTR)hMenu, L"File");

	SetMenu(hwnd, hMenubar);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HWND hwndTab;
	RECT rcClient;

	switch (msg)
	{
	case WM_CREATE:
		AddMenus(hwnd);

		hwndTab = CreateWindowEx(0, WC_TABCONTROL, L"", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
			0, 0, 0, 0, hwnd, (HMENU)IDC_TAB_CTRL, GetModuleHandle(NULL), NULL);
		break;

	case WM_SIZE:
		GetClientRect(hwnd, &rcClient);

		MoveWindow(hwndTab, 0, 0, rcClient.right, 30, TRUE);

		for (size_t i = 0; i < g_tabs.size(); ++i)
		{
			MoveWindow(g_tabs[i].hwndEdit, 0, 30, rcClient.right, rcClient.bottom - 30, TRUE);
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDM_FILE_OPEN:
			DoFileOpen(hwndTab, hwnd);
			break;

		case IDM_FILE_SAVE:
			DoFileSave(hwndTab, hwnd);
			break;

		case IDM_FILE_SAVEAS:
			DoFileSaveAs(hwndTab, hwnd);
			break;

		case IDM_FILE_EXIT:
			PostMessage(hwnd, WM_CLOSE, 0, 0);
			break;
		}
		break;

	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->idFrom == IDC_TAB_CTRL && ((LPNMHDR)lParam)->code == TCN_SELCHANGE)
		{
			int sel = GetCurrentTabIndex(hwndTab);

			SwitchToTab(hwndTab, sel);
		}
		break;

	case WM_DROPFILES:
	{
		HDROP hDrop = (HDROP)wParam;
		wchar_t szFile[MAX_PATH];

		if (DragQueryFile(hDrop, 0, szFile, MAX_PATH))
		{
			// Create a new Rich Edit control for the dropped file
			HWND hwndEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"RICHEDIT50W", L"",
				WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL,
				0, 30, 800, 600, hwnd, NULL, GetModuleHandle(NULL), NULL);

			// Set the maximum text length for the edit control so that large files are editable
			SendMessage(hwndEdit, EM_EXLIMITTEXT, 0, -1);

			// Resize the edit control to fit the window
			RECT rcClient;
			GetClientRect(hwnd, &rcClient);

			MoveWindow(hwndEdit, 0, 30, rcClient.right, rcClient.bottom - 30, TRUE);

			if (!g_hFont)
			{
				g_hFont = CreateFont(
					-MulDiv(11, GetDeviceCaps(GetDC(hwnd), LOGPIXELSY), 72),
					0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
					OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
					FIXED_PITCH | FF_MODERN, L"Consolas");
			}

			SendMessage(hwndEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);

			OpenFileToEditor(hwndEdit, szFile);

			AddTab(hwndTab, szFile, hwndEdit);
		}

		DragFinish(hDrop);
		break;
	}

	case WM_DESTROY:
		if (g_hFont)
		{
			DeleteObject(g_hFont);
			g_hFont = NULL;
		}

		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return 0;
}

void AddTab(HWND hwndTab, const wchar_t* filePath, HWND hwndEdit)
{
	TCITEM tie = { 0 };
	tie.mask = TCIF_TEXT;
	tie.pszText = (LPWSTR)filePath;
	int newIndex = (int)g_tabs.size();

	TabCtrl_InsertItem(hwndTab, newIndex, &tie);

	TabCtrl_SetCurSel(hwndTab, newIndex); // Auto-select the new tab

	EditorTab tab = { hwndEdit, {0} };
	wcsncpy_s(tab.filePath, filePath, MAX_PATH - 1);
	g_tabs.push_back(tab);

	ShowWindow(hwndEdit, SW_HIDE);

	SwitchToTab(hwndTab, newIndex);
}

void SwitchToTab(HWND hwndTab, int index)
{
	for (size_t i = 0; i < g_tabs.size(); ++i)
	{
		ShowWindow(g_tabs[i].hwndEdit, i == (size_t)index ? SW_SHOW : SW_HIDE);
	}
}

int GetCurrentTabIndex(HWND hwndTab)
{
	return TabCtrl_GetCurSel(hwndTab);
}

EditorTab* GetTabData(HWND hwndTab, int index)
{
	if (index < 0 || (size_t)index >= g_tabs.size())
		return nullptr;

	return &g_tabs[index];
}

void DoFileOpen(HWND hwndTab, HWND hwnd)
{
	OPENFILENAME ofn = { 0 };
	wchar_t szFile[MAX_PATH] = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFilter = L"Text Files (*.txt;*.log;*.json)\0*.txt;*.log;*.json\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

	if (GetOpenFileName(&ofn))
	{
		HWND hwndEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"RICHEDIT50W", L"",
			WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL,
			0, 30, 800, 600, GetParent(hwndTab), NULL, GetModuleHandle(NULL), NULL);

		SendMessage(hwndEdit, EM_EXLIMITTEXT, 0, -1);

		// Resize the edit control to fit the window
		RECT rcClient;
		GetClientRect(GetParent(hwndTab), &rcClient);

		MoveWindow(hwndEdit, 0, 30, rcClient.right, rcClient.bottom - 30, TRUE);

		if (!g_hFont)
		{
			g_hFont = CreateFont(
				-MulDiv(11, GetDeviceCaps(GetDC(hwndTab), LOGPIXELSY), 72),
				0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
				OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
				FIXED_PITCH | FF_MODERN, L"Consolas");
		}

		SendMessage(hwndEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);

		OpenFileToEditor(hwndEdit, szFile);

		AddTab(hwndTab, szFile, hwndEdit);
	}
}

void OpenFileToEditor(HWND hwndEdit, const wchar_t* szFile)
{
	std::ifstream file(szFile, std::ios::binary);
	if (!file)
		return;

	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	int wlen = MultiByteToWideChar(CP_UTF8, 0, content.c_str(), (int)content.size(), NULL, 0);
	std::wstring wcontent(wlen, 0);
	MultiByteToWideChar(CP_UTF8, 0, content.c_str(), (int)content.size(), &wcontent[0], wlen);

	SetWindowText(hwndEdit, wcontent.c_str());
}

void DoFileSave(HWND hwndTab, HWND hwnd)
{
	int idx = GetCurrentTabIndex(hwndTab);
	EditorTab* tab = GetTabData(hwndTab, idx);
	if (!tab)
		return;

	if (wcslen(tab->filePath) == 0)
	{
		DoFileSaveAs(hwndTab, hwnd);
		return;
	}

	int len = GetWindowTextLength(tab->hwndEdit);
	std::wstring wcontent(len, 0);
	GetWindowText(tab->hwndEdit, &wcontent[0], len + 1);
	int u8len = WideCharToMultiByte(CP_UTF8, 0, wcontent.c_str(), len, NULL, 0, NULL, NULL);
	std::string content(u8len, 0);
	WideCharToMultiByte(CP_UTF8, 0, wcontent.c_str(), len, &content[0], u8len, NULL, NULL);

	std::ofstream file(tab->filePath, std::ios::binary);
	file.write(content.c_str(), content.size());
}

void DoFileSaveAs(HWND hwndTab, HWND hwnd)
{
	int idx = GetCurrentTabIndex(hwndTab);
	EditorTab* tab = GetTabData(hwndTab, idx);
	if (!tab)
		return;

	OPENFILENAME ofn = { 0 };
	wchar_t szFile[MAX_PATH] = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFilter = L"Text Files (*.txt;*.log;*.json)\0*.txt;*.log;*.json\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_OVERWRITEPROMPT;

	if (GetSaveFileName(&ofn))
	{
		int len = GetWindowTextLength(tab->hwndEdit);
		std::wstring wcontent(len, 0);
		GetWindowText(tab->hwndEdit, &wcontent[0], len + 1);
		int u8len = WideCharToMultiByte(CP_UTF8, 0, wcontent.c_str(), len, NULL, 0, NULL, NULL);
		std::string content(u8len, 0);
		WideCharToMultiByte(CP_UTF8, 0, wcontent.c_str(), len, &content[0], u8len, NULL, NULL);

		std::ofstream file(szFile, std::ios::binary);
		file.write(content.c_str(), content.size());

		wcsncpy_s(tab->filePath, szFile, MAX_PATH - 1);
		TCITEM tie = { 0 };
		tie.mask = TCIF_TEXT;
		tie.pszText = szFile;
		TabCtrl_SetItem(hwndTab, idx, &tie);
	}
}
