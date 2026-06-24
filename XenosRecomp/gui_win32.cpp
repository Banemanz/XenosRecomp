#ifdef _WIN32

#include <Windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include <filesystem>
#include <string>

namespace
{
constexpr int InputEditId = 1001;
constexpr int OutputEditId = 1002;
constexpr int HeaderEditId = 1003;
constexpr int StatusTextId = 1004;

HINSTANCE g_instance = nullptr;
HWND g_inputEdit = nullptr;
HWND g_outputEdit = nullptr;
HWND g_headerEdit = nullptr;
HWND g_statusText = nullptr;
HFONT g_font = nullptr;

std::wstring getWindowText(HWND window)
{
    int length = GetWindowTextLengthW(window);
    std::wstring text(length, L'\0');
    if (length > 0)
        GetWindowTextW(window, text.data(), length + 1);
    return text;
}

void setStatus(const wchar_t* text)
{
    SetWindowTextW(g_statusText, text);
}

void setControlFont(HWND control)
{
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
}

HWND createLabel(HWND parent, const wchar_t* text, int x, int y, int width, int height)
{
    HWND label = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
        x, y, width, height, parent, nullptr, g_instance, nullptr);
    setControlFont(label);
    return label;
}

HWND createEdit(HWND parent, int id, int x, int y, int width, int height)
{
    HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        x, y, width, height, parent, reinterpret_cast<HMENU>(id), g_instance, nullptr);
    setControlFont(edit);
    return edit;
}

HWND createButton(HWND parent, const wchar_t* text, int id, int x, int y, int width, int height)
{
    HWND button = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        x, y, width, height, parent, reinterpret_cast<HMENU>(id), g_instance, nullptr);
    setControlFont(button);
    return button;
}

bool chooseFile(HWND owner, HWND target, const wchar_t* title, const wchar_t* filter)
{
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrTitle = title;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&dialog))
        return false;

    SetWindowTextW(target, path);
    return true;
}

bool chooseDirectory(HWND owner, HWND target)
{
    BROWSEINFOW browse = {};
    browse.hwndOwner = owner;
    browse.lpszTitle = L"Select input shader directory";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&browse);
    if (!item)
        return false;

    wchar_t path[MAX_PATH] = {};
    bool ok = SHGetPathFromIDListW(item, path);
    CoTaskMemFree(item);

    if (ok)
        SetWindowTextW(target, path);

    return ok;
}

bool chooseOutput(HWND owner, HWND target)
{
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrTitle = L"Select output file";
    dialog.lpstrFilter = L"HLSL or C++ output\0*.hlsl;*.cpp\0All files\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&dialog))
        return false;

    SetWindowTextW(target, path);
    return true;
}

std::wstring quoteArgument(const std::wstring& argument)
{
    std::wstring quoted = L"\"";
    for (wchar_t ch : argument)
    {
        if (ch == L'\"')
            quoted += L'\\';
        quoted += ch;
    }
    quoted += L"\"";
    return quoted;
}

void runRecompiler(HWND owner)
{
    std::wstring input = getWindowText(g_inputEdit);
    std::wstring output = getWindowText(g_outputEdit);
    std::wstring header = getWindowText(g_headerEdit);

    if (input.empty() || output.empty() || header.empty())
    {
        MessageBoxW(owner, L"Please select an input path, output path, and shader_common.h path.", L"XenosRecomp GUI", MB_ICONWARNING);
        return;
    }

    wchar_t modulePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    std::filesystem::path executable = std::filesystem::path(modulePath).parent_path() / L"XenosRecomp.exe";

    if (!std::filesystem::exists(executable))
    {
        MessageBoxW(owner, L"XenosRecomp.exe was not found next to the GUI executable.", L"XenosRecomp GUI", MB_ICONERROR);
        return;
    }

    std::wstring commandLine = quoteArgument(executable.wstring()) + L" " +
        quoteArgument(input) + L" " + quoteArgument(output) + L" " + quoteArgument(header);

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo = {};

    setStatus(L"Running XenosRecomp...");

    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startupInfo, &processInfo))
    {
        setStatus(L"Failed to start XenosRecomp.");
        MessageBoxW(owner, L"Failed to start XenosRecomp.exe.", L"XenosRecomp GUI", MB_ICONERROR);
        return;
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);

    DWORD exitCode = 1;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);

    if (exitCode == 0)
    {
        setStatus(L"Done.");
        MessageBoxW(owner, L"Recompilation finished successfully.", L"XenosRecomp GUI", MB_ICONINFORMATION);
    }
    else
    {
        setStatus(L"XenosRecomp failed.");
        MessageBoxW(owner, L"XenosRecomp exited with an error.", L"XenosRecomp GUI", MB_ICONERROR);
    }
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        g_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

        createLabel(window, L"Input shader file or directory", 16, 18, 180, 20);
        g_inputEdit = createEdit(window, InputEditId, 16, 42, 500, 24);
        createButton(window, L"File...", 2001, 526, 41, 80, 26);
        createButton(window, L"Folder...", 2002, 614, 41, 88, 26);

        createLabel(window, L"Output HLSL or shader cache C++ file", 16, 82, 260, 20);
        g_outputEdit = createEdit(window, OutputEditId, 16, 106, 500, 24);
        createButton(window, L"Browse...", 2003, 526, 105, 176, 26);

        createLabel(window, L"shader_common.h", 16, 146, 180, 20);
        g_headerEdit = createEdit(window, HeaderEditId, 16, 170, 500, 24);
        createButton(window, L"Browse...", 2004, 526, 169, 176, 26);

        createButton(window, L"Run", 2005, 16, 216, 120, 32);
        g_statusText = createLabel(window, L"Ready.", 152, 224, 550, 20);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case 2001:
            chooseFile(window, g_inputEdit, L"Select input shader file", L"Shader files\0*.*\0");
            return 0;
        case 2002:
            chooseDirectory(window, g_inputEdit);
            return 0;
        case 2003:
            chooseOutput(window, g_outputEdit);
            return 0;
        case 2004:
            chooseFile(window, g_headerEdit, L"Select shader_common.h", L"Header files\0*.h\0All files\0*.*\0");
            return 0;
        case 2005:
            runRecompiler(window);
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    g_instance = instance;

    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = L"XenosRecompGuiWindow";

    RegisterClassW(&windowClass);

    HWND window = CreateWindowExW(0, windowClass.lpszClassName, L"XenosRecomp GUI",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 734, 306,
        nullptr, nullptr, instance, nullptr);

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

#endif
