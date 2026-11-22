#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#define WINVER 0x0600

#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <objbase.h>
#include <vector>
#include <string>
#include <algorithm>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

static const wchar_t* CLASS_NAME = L"ShadowPlayMainWindow";
static const wchar_t* APP_TITLE = L"SHADOWPLAY";
static const int ID_REFRESH = 1001;
static const int ID_LAUNCH = 1002;
static const int ID_GAMES = 1003;

struct GameEntry {
    std::wstring name;
    std::wstring shortcut;
    std::wstring target;
};

static std::vector<GameEntry> g_games;
static HWND g_list = NULL;
static HWND g_status = NULL;
static HFONT g_font = NULL;
static HFONT g_titleFont = NULL;

static std::wstring BaseName(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    std::wstring name = (p == std::wstring::npos) ? path : path.substr(p + 1);
    size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos) name.erase(dot);
    return name;
}

static bool ResolveShortcut(const std::wstring& linkPath, std::wstring& target) {
    IShellLinkW* link = NULL;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                  IID_IShellLinkW, reinterpret_cast<void**>(&link));
    if (FAILED(hr) || !link) return false;

    IPersistFile* file = NULL;
    hr = link->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&file));
    if (SUCCEEDED(hr) && file) {
        hr = file->Load(linkPath.c_str(), STGM_READ);
        if (SUCCEEDED(hr)) {
            WIN32_FIND_DATAW findData;
            wchar_t buffer[MAX_PATH];
            buffer[0] = 0;
            hr = link->GetPath(buffer, MAX_PATH, &findData, SLGP_UNCPRIORITY);
            if (SUCCEEDED(hr) && buffer[0]) target = buffer;
        }
        file->Release();
    }
    link->Release();
    return !target.empty();
}

static void AddShortcut(const std::wstring& path) {
    if (path.size() < 4 || _wcsicmp(path.c_str() + path.size() - 4, L".lnk") != 0)
        return;

    for (size_t i = 0; i < g_games.size(); ++i)
        if (_wcsicmp(g_games[i].shortcut.c_str(), path.c_str()) == 0) return;

    std::wstring target;
    if (!ResolveShortcut(path, target)) return;

    GameEntry entry;
    entry.name = BaseName(path);
    entry.shortcut = path;
    entry.target = target;
    g_games.push_back(entry);
}

static void ScanFolder(const std::wstring& folder, int depth) {
    if (depth > 12 || folder.empty()) return;
    std::wstring pattern = folder + L"\\*";
    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0)
            continue;
        std::wstring full = folder + L"\\" + data.cFileName;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ScanFolder(full, depth + 1);
        } else {
            AddShortcut(full);
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
}

static void ScanKnownFolder(int csidl) {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, csidl, NULL, SHGFP_TYPE_CURRENT, path)) && path[0])
        ScanFolder(path, 0);
}

static void SetStatus(const std::wstring& text) {
    if (g_status) SetWindowTextW(g_status, text.c_str());
}

static bool CompareGames(const GameEntry& a, const GameEntry& b) {
    return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
}

static void RefreshLibrary() {
    g_games.clear();
    ScanKnownFolder(CSIDL_DESKTOPDIRECTORY);
    ScanKnownFolder(CSIDL_COMMON_DESKTOPDIRECTORY);
    ScanKnownFolder(CSIDL_STARTMENU);
    ScanKnownFolder(CSIDL_COMMON_STARTMENU);

    std::sort(g_games.begin(), g_games.end(), CompareGames);

    SendMessageW(g_list, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < g_games.size(); ++i)
        SendMessageW(g_list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(g_games[i].name.c_str()));

    wchar_t message[128];
    wsprintfW(message, L"Знайдено ярликів ігор: %u", (unsigned)g_games.size());
    SetStatus(message);
}

static void LaunchSelected() {
    int index = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
    if (index == LB_ERR || index < 0 || (size_t)index >= g_games.size()) {
        MessageBoxW(GetParent(g_list), L"Спочатку виберіть гру зі списку.",
                    APP_TITLE, MB_OK | MB_ICONINFORMATION);
        return;
    }

    HINSTANCE result = ShellExecuteW(NULL, L"open", g_games[index].shortcut.c_str(),
                                     NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        ShellExecuteW(NULL, L"open", g_games[index].target.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
}

static void SetControlFont(HWND control, HFONT font) {
    SendMessageW(control, WM_SETFONT, (WPARAM)font, TRUE);
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_font = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_titleFont = CreateFontW(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        HWND title = CreateWindowW(L"STATIC", APP_TITLE, WS_CHILD | WS_VISIBLE,
            24, 18, 400, 46, hwnd, NULL, NULL, NULL);
        SetControlFont(title, g_titleFont);

        HWND subtitle = CreateWindowW(L"STATIC", L"ІГРОВА БІБЛІОТЕКА",
            WS_CHILD | WS_VISIBLE, 27, 65, 300, 25, hwnd, NULL, NULL, NULL);
        SetControlFont(subtitle, g_font);

        g_list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            24, 105, 632, 330, hwnd, (HMENU)ID_GAMES, NULL, NULL);
        SetControlFont(g_list, g_font);

        HWND refresh = CreateWindowW(L"BUTTON", L"ОНОВИТИ БІБЛІОТЕКУ",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 24, 452, 220, 42,
            hwnd, (HMENU)ID_REFRESH, NULL, NULL);
        SetControlFont(refresh, g_font);

        HWND launch = CreateWindowW(L"BUTTON", L"ЗАПУСТИТИ ГРУ",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 256, 452, 180, 42,
            hwnd, (HMENU)ID_LAUNCH, NULL, NULL);
        SetControlFont(launch, g_font);

        g_status = CreateWindowW(L"STATIC", L"Сканування ярликів...",
            WS_CHILD | WS_VISIBLE, 24, 510, 632, 28, hwnd, NULL, NULL, NULL);
        SetControlFont(g_status, g_font);

        RefreshLibrary();
        return 0;
    }
    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (g_list) MoveWindow(g_list, 24, 105, width - 48, height - 190, TRUE);
        HWND refresh = GetDlgItem(hwnd, ID_REFRESH);
        HWND launch = GetDlgItem(hwnd, ID_LAUNCH);
        if (refresh) MoveWindow(refresh, 24, height - 82, 220, 42, TRUE);
        if (launch) MoveWindow(launch, 256, height - 82, 180, 42, TRUE);
        if (g_status) MoveWindow(g_status, 24, height - 32, width - 48, 24, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_REFRESH && HIWORD(wParam) == BN_CLICKED) {
            RefreshLibrary();
            return 0;
        }
        if ((LOWORD(wParam) == ID_LAUNCH && HIWORD(wParam) == BN_CLICKED) ||
            (LOWORD(wParam) == ID_GAMES && HIWORD(wParam) == LBN_DBLCLK)) {
            LaunchSelected();
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)wParam;
        SetTextColor(dc, RGB(245, 245, 245));
        SetBkColor(dc, RGB(20, 20, 20));
        static HBRUSH brush = CreateSolidBrush(RGB(20, 20, 20));
        return (LRESULT)brush;
    }
    case WM_ERASEBKGND: {
        RECT r;
        GetClientRect(hwnd, &r);
        HBRUSH brush = CreateSolidBrush(RGB(10, 10, 10));
        FillRect((HDC)wParam, &r, brush);
        DeleteObject(brush);
        return 1;
    }
    case WM_DESTROY:
        if (g_font) DeleteObject(g_font);
        if (g_titleFont) DeleteObject(g_titleFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
    HRESULT comResult = CoInitialize(NULL);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassExW(&wc)) {
        if (SUCCEEDED(comResult)) CoUninitialize();
        return 1;
    }

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, APP_TITLE,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 700, 610,
        NULL, NULL, instance, NULL);
    if (!hwnd) {
        if (SUCCEEDED(comResult)) CoUninitialize();
        return 1;
    }

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (SUCCEEDED(comResult)) CoUninitialize();
    return (int)msg.wParam;
}
