// Minimal dark-mode text editor using the native Win32 API.
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <stdlib.h>
#include <wchar.h>

#define ID_EDIT       1001
#define ID_FILE_NEW   2001
#define ID_FILE_OPEN  2002
#define ID_FILE_SAVE  2003
#define ID_FILE_SAVEAS 2004
#define ID_FILE_EXIT  2005
#define MAX_DOCUMENT_BYTES (16u * 1024u * 1024u)

#define COLOR_BG   RGB(30, 30, 30)
#define COLOR_TEXT RGB(220, 220, 220)

static HWND hEdit;
static HBRUSH hBrushBG;
static HBRUSH hMenuBrush;
static HFONT hFont;
static HMENU hFileMenu;
static HACCEL hAccelerators;
static WCHAR currentFile[MAX_PATH];
static BOOL fileModified;

static void CopyPath(WCHAR *destination, size_t capacity, LPCWSTR source) {
    if (!capacity) return;
    wcsncpy(destination, source, capacity - 1);
    destination[capacity - 1] = L'\0';
}

static void ShowFileError(HWND hwnd, LPCWSTR action, LPCWSTR path) {
    DWORD error = GetLastError();
    WCHAR message[768];
    WCHAR detail[384];
    if (!error || FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                NULL, error, 0, detail, (DWORD)(sizeof(detail) / sizeof(detail[0])), NULL) == 0)
        CopyPath(detail, sizeof(detail) / sizeof(detail[0]), L"Windows reported an unspecified error.");
    wsprintfW(message, L"%s\n\n%s\n%s", action, path, detail);
    MessageBoxW(hwnd, message, L"Dark Notepad", MB_OK | MB_ICONERROR);
}

static void UpdateTitle(HWND hwnd) {
    WCHAR title[MAX_PATH + 48];
    LPCWSTR name = currentFile[0] ? currentFile : L"Untitled";
    wsprintfW(title, L"%s%s - Dark Notepad", fileModified ? L"*" : L"", name);
    SetWindowTextW(hwnd, title);
}

static BOOL ConfirmDiscard(HWND hwnd) {
    if (!fileModified) return TRUE;
    int answer = MessageBoxW(hwnd, L"Save changes before continuing?", L"Dark Notepad",
                             MB_YESNOCANCEL | MB_ICONWARNING);
    if (answer == IDCANCEL) return FALSE;
    if (answer == IDYES) {
        SendMessageW(hwnd, WM_COMMAND, ID_FILE_SAVE, 0);
        return !fileModified;
    }
    return TRUE;
}

static void NewFile(HWND hwnd) {
    if (!ConfirmDiscard(hwnd)) return;
    SetWindowTextW(hEdit, L"");
    currentFile[0] = L'\0';
    fileModified = FALSE;
    UpdateTitle(hwnd);
}

static WCHAR *DecodeFile(const BYTE *bytes, DWORD size, int *characterCount) {
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    const char *input = (const char *)bytes;
    int inputLength = (int)size;
    WCHAR *wide;
    int count;

    if (size >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        input = (const char *)(bytes + 2);
        inputLength = (int)(size - 2);
        if (inputLength % 2) return NULL;
        count = inputLength / 2;
        wide = (WCHAR *)malloc(((size_t)count + 1) * sizeof(WCHAR));
        if (!wide) return NULL;
        for (int i = 0; i < count; ++i) wide[i] = (WCHAR)((const BYTE *)input)[i * 2] | ((WCHAR)((const BYTE *)input)[i * 2 + 1] << 8);
        wide[count] = L'\0';
        *characterCount = count;
        return wide;
    }
    if (size >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF) {
        input = (const char *)(bytes + 2);
        inputLength = (int)(size - 2);
        if (inputLength % 2) return NULL;
        count = inputLength / 2;
        wide = (WCHAR *)malloc(((size_t)count + 1) * sizeof(WCHAR));
        if (!wide) return NULL;
        for (int i = 0; i < count; ++i) wide[i] = ((WCHAR)((const BYTE *)input)[i * 2] << 8) | (WCHAR)((const BYTE *)input)[i * 2 + 1];
        wide[count] = L'\0';
        *characterCount = count;
        return wide;
    }
    if (size >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        input += 3;
        inputLength -= 3;
    }
    if (!inputLength) {
        wide = (WCHAR *)calloc(1, sizeof(WCHAR));
        *characterCount = 0;
        return wide;
    }
    count = MultiByteToWideChar(codePage, flags, input, inputLength, NULL, 0);
    if (!count) {
        codePage = CP_ACP;
        flags = 0;
        count = MultiByteToWideChar(codePage, flags, input, inputLength, NULL, 0);
    }
    if (!count) return NULL;
    wide = (WCHAR *)malloc(((size_t)count + 1) * sizeof(WCHAR));
    if (!wide) return NULL;
    if (!MultiByteToWideChar(codePage, flags, input, inputLength, wide, count)) {
        free(wide);
        return NULL;
    }
    wide[count] = L'\0';
    *characterCount = count;
    return wide;
}

static BOOL LoadFileIntoEditor(HWND hwnd, LPCWSTR filename) {
    HANDLE file = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        ShowFileError(hwnd, L"Could not open the file:", filename);
        return FALSE;
    }
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(file, &fileSize)) {
        CloseHandle(file);
        ShowFileError(hwnd, L"Could not determine the file size:", filename);
        return FALSE;
    }
    if (fileSize.QuadPart > MAX_DOCUMENT_BYTES) {
        CloseHandle(file);
        MessageBoxW(hwnd, L"This editor supports files up to 16 MiB.", L"Dark Notepad", MB_OK | MB_ICONWARNING);
        return FALSE;
    }
    DWORD size = (DWORD)fileSize.QuadPart;
    BYTE *bytes = (BYTE *)malloc((size_t)size + 1);
    if (!bytes) {
        CloseHandle(file);
        MessageBoxW(hwnd, L"Not enough memory to open this file.", L"Dark Notepad", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    DWORD total = 0;
    while (total < size) {
        DWORD read = 0;
        if (!ReadFile(file, bytes + total, size - total, &read, NULL) || read == 0) break;
        total += read;
    }
    CloseHandle(file);
    if (total != size) {
        free(bytes);
        ShowFileError(hwnd, L"Could not read the complete file:", filename);
        return FALSE;
    }
    int characters = 0;
    WCHAR *wide = DecodeFile(bytes, size, &characters);
    free(bytes);
    BOOL containsNul = FALSE;
    if (wide) for (int i = 0; i < characters; ++i) if (!wide[i]) { containsNul = TRUE; break; }
    if (!wide || containsNul || characters > 16 * 1024 * 1024) {
        free(wide);
        MessageBoxW(hwnd, containsNul ? L"Files containing embedded NUL characters are not supported." :
                    L"The file could not be decoded or is too large for the editor.",
                    L"Dark Notepad", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    SendMessageW(hEdit, EM_SETLIMITTEXT, (WPARAM)characters + 1, 0);
    SetWindowTextW(hEdit, wide);
    free(wide);
    CopyPath(currentFile, MAX_PATH, filename);
    fileModified = FALSE;
    UpdateTitle(hwnd);
    return TRUE;
}

static BOOL SaveToFile(HWND hwnd, LPCWSTR filename) {
    int length = GetWindowTextLengthW(hEdit);
    WCHAR *wide = (WCHAR *)malloc(((size_t)length + 1) * sizeof(WCHAR));
    if (!wide) {
        MessageBoxW(hwnd, L"Not enough memory to save this file.", L"Dark Notepad", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    GetWindowTextW(hEdit, wide, length + 1);
    int bytesNeeded = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, length, NULL, 0, NULL, NULL);
    if (length && !bytesNeeded) {
        free(wide);
        MessageBoxW(hwnd, L"The document contains text that cannot be saved as UTF-8.", L"Dark Notepad", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    char *utf8 = (char *)malloc((size_t)bytesNeeded + 1);
    if (!utf8) {
        free(wide);
        MessageBoxW(hwnd, L"Not enough memory to save this file.", L"Dark Notepad", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    if (bytesNeeded && !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, length, utf8, bytesNeeded, NULL, NULL)) {
        free(wide); free(utf8);
        MessageBoxW(hwnd, L"Could not convert the document to UTF-8.", L"Dark Notepad", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    HANDLE file = CreateFileW(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        free(wide); free(utf8);
        ShowFileError(hwnd, L"Could not save the file:", filename);
        return FALSE;
    }
    DWORD total = 0;
    BOOL ok = TRUE;
    while (total < (DWORD)bytesNeeded) {
        DWORD written = 0;
        if (!WriteFile(file, utf8 + total, (DWORD)bytesNeeded - total, &written, NULL) || written == 0) {
            ok = FALSE;
            break;
        }
        total += written;
    }
    if (!CloseHandle(file)) ok = FALSE;
    free(wide); free(utf8);
    if (!ok) {
        ShowFileError(hwnd, L"Could not write the complete file:", filename);
        return FALSE;
    }
    CopyPath(currentFile, MAX_PATH, filename);
    fileModified = FALSE;
    UpdateTitle(hwnd);
    return TRUE;
}

static BOOL ChooseSavePath(HWND hwnd, WCHAR *filename) {
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Text files (*.txt)\0*.txt\0All files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    return GetSaveFileNameW(&ofn);
}

static BOOL SaveFile(HWND hwnd) {
    if (!currentFile[0]) {
        WCHAR filename[MAX_PATH] = L"";
        return ChooseSavePath(hwnd, filename) ? SaveToFile(hwnd, filename) : FALSE;
    }
    return SaveToFile(hwnd, currentFile);
}

static void DoOpenFile(HWND hwnd) {
    if (!ConfirmDiscard(hwnd)) return;
    WCHAR filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Text files (*.txt)\0*.txt\0All files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) LoadFileIntoEditor(hwnd, filename);
}

static void SaveFileAs(HWND hwnd) {
    WCHAR filename[MAX_PATH] = L"";
    if (ChooseSavePath(hwnd, filename)) SaveToFile(hwnd, filename);
}

static void EnableDarkTitleBar(HWND hwnd) {
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    COLORREF border = RGB(24, 24, 24);
    DwmSetWindowAttribute(hwnd, 34, &border, sizeof(border));
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static void MeasureMenuItem(MEASUREITEMSTRUCT *item) {
    LPCWSTR text = item->itemData == (ULONG_PTR)hFileMenu ? L"File" : (LPCWSTR)item->itemData;
    HDC dc = GetDC(NULL);
    SIZE size = { 0, 0 };
    if (dc) {
        GetTextExtentPoint32W(dc, text, (int)wcslen(text), &size);
        ReleaseDC(NULL, dc);
    }
    item->itemWidth = size.cx + 24;
    item->itemHeight = size.cy + 10;
}

static void DrawMenuItem(const DRAWITEMSTRUCT *item) {
    LPCWSTR text = item->itemData == (ULONG_PTR)hFileMenu ? L"File" : (LPCWSTR)item->itemData;
    BOOL selected = (item->itemState & ODS_SELECTED) != 0;
    HBRUSH brush = CreateSolidBrush(selected ? RGB(55, 55, 55) : COLOR_BG);
    FillRect(item->hDC, &item->rcItem, brush);
    DeleteObject(brush);
    SetTextColor(item->hDC, COLOR_TEXT);
    SetBkMode(item->hDC, TRANSPARENT);
    RECT rect = item->rcItem;
    rect.left += 10;
    DrawTextW(item->hDC, text, -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            hBrushBG = CreateSolidBrush(COLOR_BG);
            hMenuBrush = CreateSolidBrush(COLOR_BG);
            hEdit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, 0, 0, 0, 0, hwnd,
                (HMENU)(INT_PTR)ID_EDIT, GetModuleHandleW(NULL), NULL);
            SendMessageW(hEdit, EM_SETLIMITTEXT, 16 * 1024 * 1024, 0);
            HDC screen = GetDC(NULL);
            int dpi = screen ? GetDeviceCaps(screen, LOGPIXELSY) : 96;
            if (screen) ReleaseDC(NULL, screen);
            hFont = CreateFontW(-MulDiv(14, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH, L"Consolas");
            if (hFont) SendMessageW(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

            HMENU menu = CreateMenu();
            hFileMenu = CreatePopupMenu();
            AppendMenuW(hFileMenu, MF_OWNERDRAW, ID_FILE_NEW, (LPCWSTR)L"New\tCtrl+N");
            AppendMenuW(hFileMenu, MF_OWNERDRAW, ID_FILE_OPEN, (LPCWSTR)L"Open...\tCtrl+O");
            AppendMenuW(hFileMenu, MF_OWNERDRAW, ID_FILE_SAVE, (LPCWSTR)L"Save\tCtrl+S");
            AppendMenuW(hFileMenu, MF_OWNERDRAW, ID_FILE_SAVEAS, (LPCWSTR)L"Save As...");
            AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hFileMenu, MF_OWNERDRAW, ID_FILE_EXIT, (LPCWSTR)L"Exit");
            AppendMenuW(menu, MF_POPUP | MF_OWNERDRAW, (UINT_PTR)hFileMenu, (LPCWSTR)L"File");
            SetMenu(hwnd, menu);
            MENUINFO info = { sizeof(info) };
            info.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
            info.hbrBack = hMenuBrush;
            SetMenuInfo(menu, &info);
            EnableDarkTitleBar(hwnd);
            UpdateTitle(hwnd);
            return 0;
        }
        case WM_SIZE: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            MoveWindow(hEdit, 0, 0, rect.right, rect.bottom, TRUE);
            return 0;
        }
        case WM_CTLCOLOREDIT:
            SetTextColor((HDC)wParam, COLOR_TEXT);
            SetBkColor((HDC)wParam, COLOR_BG);
            return (LRESULT)hBrushBG;
        case WM_ERASEBKGND: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect((HDC)wParam, &rect, hBrushBG);
            return 1;
        }
        case WM_MEASUREITEM:
            if (((MEASUREITEMSTRUCT *)lParam)->CtlType == ODT_MENU) MeasureMenuItem((MEASUREITEMSTRUCT *)lParam);
            return TRUE;
        case WM_DRAWITEM:
            if (((DRAWITEMSTRUCT *)lParam)->CtlType == ODT_MENU) DrawMenuItem((DRAWITEMSTRUCT *)lParam);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_FILE_NEW: NewFile(hwnd); return 0;
                case ID_FILE_OPEN: DoOpenFile(hwnd); return 0;
                case ID_FILE_SAVE: SaveFile(hwnd); return 0;
                case ID_FILE_SAVEAS: SaveFileAs(hwnd); return 0;
                case ID_FILE_EXIT: SendMessageW(hwnd, WM_CLOSE, 0, 0); return 0;
                case ID_EDIT:
                    if (HIWORD(wParam) == EN_CHANGE) { fileModified = TRUE; UpdateTitle(hwnd); }
                    return 0;
            }
            break;
        case WM_CLOSE:
            if (ConfirmDiscard(hwnd)) DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (hBrushBG) DeleteObject(hBrushBG);
            if (hMenuBrush) DeleteObject(hMenuBrush);
            if (hFont) DeleteObject(hFont);
            if (hAccelerators) DestroyAcceleratorTable(hAccelerators);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static HACCEL CreateAppAccelerators(void) {
    ACCEL entries[] = {
        { FVIRTKEY | FCONTROL, 'N', ID_FILE_NEW },
        { FVIRTKEY | FCONTROL, 'O', ID_FILE_OPEN },
        { FVIRTKEY | FCONTROL, 'S', ID_FILE_SAVE }
    };
    return CreateAcceleratorTableW(entries, 3);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR args, int showCommand) {
    (void)previous;
    SetProcessDPIAware();
    WNDCLASSEXW windowClass = { sizeof(windowClass) };
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"DarkNotepadClass";
    windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    windowClass.hbrBackground = CreateSolidBrush(COLOR_BG);
    windowClass.hIcon = LoadIconW(instance, L"IDI_APPICON");
    if (!windowClass.hIcon) windowClass.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    if (!RegisterClassExW(&windowClass)) return 1;
    HWND hwnd = CreateWindowExW(0, windowClass.lpszClassName, L"Dark Notepad", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, instance, NULL);
    if (!hwnd) return 1;
    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);
    hAccelerators = CreateAppAccelerators();
    if (args && args[0]) {
        WCHAR path[MAX_PATH];
        LPCWSTR start = args;
        while (*start == L' ' || *start == L'\t') ++start;
        if (*start == L'"') {
            ++start;
            size_t n = 0;
            while (start[n] && start[n] != L'"' && n < MAX_PATH - 1) { path[n] = start[n]; ++n; }
            path[n] = L'\0';
        } else {
            CopyPath(path, MAX_PATH, start);
        }
        if (path[0]) LoadFileIntoEditor(hwnd, path);
    }
    MSG msg;
    int result;
    while ((result = GetMessageW(&msg, NULL, 0, 0)) > 0) {
        if (!hAccelerators || !TranslateAcceleratorW(hwnd, hAccelerators, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return result == -1 ? 1 : (int)msg.wParam;
}
