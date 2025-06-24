#include <windows.h>
#include <commdlg.h>
#include <fstream>
#include <string>
#include <richedit.h>

#define IDC_MAIN_EDIT 101
#define IDM_FILE_OPEN 102
#define IDM_FILE_SAVEAS 103
#define IDM_FILE_EXIT 104
#define IDM_FILE_SAVE 105

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void DoFileOpen(HWND hwndEdit, HWND hwnd);
void DoFileSaveAs(HWND hwndEdit, HWND hwnd);
void OpenFileToEditor(HWND hwndEdit, const wchar_t* szFile);
void DoFileSave(HWND hwndEdit, HWND hwnd);

// Global font handle
HFONT g_hFont = NULL;

// Track the currently open file path
template<size_t N>
void wstrcpy_safe(wchar_t(&dst)[N], const wchar_t* src) {
	wcsncpy_s(dst, N, src, N - 1);
	dst[N - 1] = 0;
}

wchar_t g_currentFile[MAX_PATH] = { 0 };

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	// Enable DPI awareness
	SetProcessDPIAware();

	// Load Rich Edit library
	LoadLibraryW(L"Msftedit.dll");

	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"SimpleTextEditor";
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);

	RegisterClass(&wc);

	HWND hwnd = CreateWindow(wc.lpszClassName, L"Simple Text Editor", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, NULL, NULL, hInstance, NULL);

	// Enable drag and drop
	DragAcceptFiles(hwnd, TRUE);

	MSG msg;

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);

		DispatchMessage(&msg);
	}

	return 0;
}

void AddMenus(HWND hwnd) {
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

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	static HWND hwndEdit;

	switch (msg) {
	case WM_CREATE:
		AddMenus(hwnd);

		hwndEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"RICHEDIT50W", L"",
			WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL,
			0, 0, 0, 0, hwnd, (HMENU)IDC_MAIN_EDIT, GetModuleHandle(NULL), NULL);

		// Set Consolas 11pt font
		if (!g_hFont) {
			g_hFont = CreateFont(
				-MulDiv(11, GetDeviceCaps(GetDC(hwnd), LOGPIXELSY), 72),
				0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
				OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
				FIXED_PITCH | FF_MODERN, L"Consolas");
		}

		SendMessage(hwndEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
		break;
	case WM_SIZE:
		MoveWindow(hwndEdit, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
		break;
	case WM_KEYDOWN:
		if ((GetKeyState(VK_CONTROL) & 0x8000) && (wParam == 'S' || wParam == 's')) {
			DoFileSave(hwndEdit, hwnd);
			return 0;
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDM_FILE_OPEN:
			DoFileOpen(hwndEdit, hwnd);
			break;
		case IDM_FILE_SAVE:
			DoFileSave(hwndEdit, hwnd);
			break;
		case IDM_FILE_SAVEAS:
			DoFileSaveAs(hwndEdit, hwnd);
			break;
		case IDM_FILE_EXIT:
			PostMessage(hwnd, WM_CLOSE, 0, 0);
			break;
		}
		break;
	case WM_DROPFILES: {
		HDROP hDrop = (HDROP)wParam;
		wchar_t szFile[MAX_PATH];
		if (DragQueryFile(hDrop, 0, szFile, MAX_PATH)) {
			const wchar_t* ext = wcsrchr(szFile, L'.');

			OpenFileToEditor(hwndEdit, szFile);
		}

		DragFinish(hDrop);
		break;
	}
	case WM_DESTROY:
		if (g_hFont) {
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

void DoFileOpen(HWND hwndEdit, HWND hwnd) {

	OPENFILENAME ofn = { 0 };
	wchar_t szFile[MAX_PATH] = { 0 };

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFilter = L"Text Files (*.txt;*.log;*.json)\0*.txt;*.log;*.json\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

	if (GetOpenFileName(&ofn)) {
		OpenFileToEditor(hwndEdit, szFile);

		wstrcpy_safe(g_currentFile, szFile);
	}
}

void OpenFileToEditor(HWND hwndEdit, const wchar_t* szFile) {
	std::ifstream file(szFile, std::ios::binary);

	if (!file) return;

	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	int wlen = MultiByteToWideChar(CP_UTF8, 0, content.c_str(), (int)content.size(), NULL, 0);

	std::wstring wcontent(wlen, 0);

	MultiByteToWideChar(CP_UTF8, 0, content.c_str(), (int)content.size(), &wcontent[0], wlen);

	SetWindowText(hwndEdit, wcontent.c_str());
}

void DoFileSave(HWND hwndEdit, HWND hwnd) {
	if (g_currentFile[0] == 0) {
		DoFileSaveAs(hwndEdit, hwnd);
		return;
	}
	int len = GetWindowTextLength(hwndEdit);
	std::wstring wcontent(len, 0);
	GetWindowText(hwndEdit, &wcontent[0], len + 1);
	int u8len = WideCharToMultiByte(CP_UTF8, 0, wcontent.c_str(), len, NULL, 0, NULL, NULL);
	std::string content(u8len, 0);
	WideCharToMultiByte(CP_UTF8, 0, wcontent.c_str(), len, &content[0], u8len, NULL, NULL);
	std::ofstream file(g_currentFile, std::ios::binary);
	file.write(content.c_str(), content.size());
}

void DoFileSaveAs(HWND hwndEdit, HWND hwnd) {
	OPENFILENAME ofn = { 0 };
	wchar_t szFile[MAX_PATH] = { 0 };

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFilter = L"Text Files (*.txt;*.log;*.json)\0*.txt;*.log;*.json\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_OVERWRITEPROMPT;

	if (GetSaveFileName(&ofn)) {

		int len = GetWindowTextLength(hwndEdit);

		std::wstring wcontent(len, 0);
		GetWindowText(hwndEdit, &wcontent[0], len + 1);

		int u8len = WideCharToMultiByte(CP_UTF8, 0, wcontent.c_str(), len, NULL, 0, NULL, NULL);
		std::string content(u8len, 0);

		WideCharToMultiByte(CP_UTF8, 0, wcontent.c_str(), len, &content[0], u8len, NULL, NULL);

		std::ofstream file(szFile, std::ios::binary);
		file.write(content.c_str(), content.size());

		wstrcpy_safe(g_currentFile, szFile);
	}
}
