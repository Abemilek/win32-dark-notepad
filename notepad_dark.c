// ============================================================
// Bloc de notas minimalista en modo oscuro - C + Win32 API pura
// Sin dependencias externas, solo librerías del sistema Windows.
// ============================================================

#include <windows.h>
#include <commdlg.h>
#include <dwmapi.h>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dwmapi.lib")

#define ID_EDIT      1001
#define ID_FILE_NEW  2001
#define ID_FILE_OPEN 2002
#define ID_FILE_SAVE 2003
#define ID_FILE_SAVEAS 2004
#define ID_FILE_EXIT 2005

// Colores del tema oscuro (estilo similar a apps modernas)
#define COLOR_BG      RGB(30, 30, 30)     // fondo del editor
#define COLOR_TEXT    RGB(220, 220, 220)  // texto
#define MY_BG_WINDOW  RGB(24, 24, 24)     // fondo de la ventana

HWND hEdit;
HBRUSH hBrushBG;
HBRUSH hMenuBrush;
WCHAR currentFile[MAX_PATH] = L"";
BOOL fileModified = FALSE;

// ---------- Utilidades de archivo ----------

void UpdateTitle(HWND hwnd) {
    WCHAR title[MAX_PATH + 32];
    LPCWSTR name = (currentFile[0] == L'\0') ? L"Sin título" : currentFile;
    wsprintfW(title, L"%s%s - Bloc de Notas Oscuro", fileModified ? L"*" : L"", name);
    SetWindowTextW(hwnd, title);
}

void NewFile(HWND hwnd) {
    SetWindowTextW(hEdit, L"");
    currentFile[0] = L'\0';
    fileModified = FALSE;
    UpdateTitle(hwnd);
}

void LoadFileIntoEditor(HWND hwnd, LPCWSTR filename) {
    HANDLE hFile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD size = GetFileSize(hFile, NULL);
        char *buffer = (char*)malloc(size + 1);
        DWORD read;
        ReadFile(hFile, buffer, size, &read, NULL);
        buffer[read] = '\0';
        CloseHandle(hFile);

        int wlen = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, NULL, 0);
        WCHAR *wbuffer = (WCHAR*)malloc(wlen * sizeof(WCHAR));
        MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wbuffer, wlen);

        SetWindowTextW(hEdit, wbuffer);
        free(buffer);
        free(wbuffer);

        wcscpy(currentFile, filename);
        fileModified = FALSE;
        UpdateTitle(hwnd);
    }
}

void DoOpenFile(HWND hwnd) {
    WCHAR filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Archivos de texto (*.txt)\0*.txt\0Todos los archivos\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        LoadFileIntoEditor(hwnd, filename);
    }
}

void SaveToFile(HWND hwnd, LPCWSTR filename) {
    int len = GetWindowTextLengthW(hEdit);
    WCHAR *wbuffer = (WCHAR*)malloc((len + 1) * sizeof(WCHAR));
    GetWindowTextW(hEdit, wbuffer, len + 1);

    int ulen = WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, NULL, 0, NULL, NULL);
    char *ubuffer = (char*)malloc(ulen);
    WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, ubuffer, ulen, NULL, NULL);

    HANDLE hFile = CreateFileW(filename, GENERIC_WRITE, 0, NULL,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hFile, ubuffer, ulen - 1, &written, NULL);
        CloseHandle(hFile);
        wcscpy(currentFile, filename);
        fileModified = FALSE;
        UpdateTitle(hwnd);
    }
    free(wbuffer);
    free(ubuffer);
}

void SaveFile(HWND hwnd) {
    if (currentFile[0] == L'\0') {
        // Si no hay archivo actual, comportarse como "Guardar como"
        WCHAR filename[MAX_PATH] = L"";
        OPENFILENAMEW ofn = { sizeof(ofn) };
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = L"Archivos de texto (*.txt)\0*.txt\0Todos los archivos\0*.*\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrDefExt = L"txt";
        ofn.Flags = OFN_OVERWRITEPROMPT;
        if (GetSaveFileNameW(&ofn)) {
            SaveToFile(hwnd, filename);
        }
    } else {
        SaveToFile(hwnd, currentFile);
    }
}

void SaveFileAs(HWND hwnd) {
    WCHAR filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Archivos de texto (*.txt)\0*.txt\0Todos los archivos\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&ofn)) {
        SaveToFile(hwnd, filename);
    }
}

// ---------- Modo oscuro para la barra de título (Windows 10 1809+) ----------

void EnableDarkTitleBar(HWND hwnd) {
    BOOL dark = TRUE;
    // 20 = DWMWA_USE_IMMERSIVE_DARK_MODE en versiones recientes de Windows 10/11
    // 19 = valor usado en builds anteriores de Windows 10 1809/1903
    if (DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark)) != S_OK) {
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    }

    // DWMWA_BORDER_COLOR (34) - fuerza el color del marco exterior a oscuro.
    // Solo soportado en Windows 11 (build 22000+). En Windows 10 la llamada
    // simplemente falla y se ignora (no rompe nada).
    COLORREF borderColor = RGB(24, 24, 24);
    DwmSetWindowAttribute(hwnd, 34, &borderColor, sizeof(borderColor));

    // Fuerza a Windows a repintar el marco no-cliente ya con los
    // atributos aplicados (sin esto, a veces el borde tarda en actualizarse
    // o se queda con el color por defecto).
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// ---------- Procedimiento de ventana ----------

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            hBrushBG = CreateSolidBrush(COLOR_BG);

            hEdit = CreateWindowExW(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE |
                ES_MULTILINE | ES_AUTOVSCROLL,
                0, 0, 0, 0, hwnd, (HMENU)ID_EDIT,
                GetModuleHandle(NULL), NULL);

            // Fuente legible y moderna, calculada según el DPI real de la
            // pantalla (evita que Windows "estire" el texto y se vea pixelado
            // en pantallas con escalado 125%/150%/etc.)
            HDC hdcScreen = GetDC(NULL);
            int dpi = GetDeviceCaps(hdcScreen, LOGPIXELSY);
            ReleaseDC(NULL, hdcScreen);
            HFONT hFont = CreateFontW(-MulDiv(14, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
            SendMessageW(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Menú
            HMENU hMenu = CreateMenu();
            HMENU hFileMenu = CreatePopupMenu();
            AppendMenuW(hFileMenu, MF_OWNERDRAW, ID_FILE_NEW, (LPCWSTR)L"Nuevo\tCtrl+N");
            AppendMenuW(hFileMenu, MF_OWNERDRAW, ID_FILE_OPEN, (LPCWSTR)L"Abrir...\tCtrl+O");
            AppendMenuW(hFileMenu, MF_OWNERDRAW, ID_FILE_SAVE, (LPCWSTR)L"Guardar\tCtrl+S");
            AppendMenuW(hFileMenu, MF_OWNERDRAW, ID_FILE_SAVEAS, (LPCWSTR)L"Guardar como...");
            AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hFileMenu, MF_OWNERDRAW, ID_FILE_EXIT, (LPCWSTR)L"Salir");
            AppendMenuW(hMenu, MF_POPUP | MF_OWNERDRAW, (UINT_PTR)hFileMenu, (LPCWSTR)L"Archivo");
            SetMenu(hwnd, hMenu);

            // Fondo oscuro para toda la barra de menú y sus submenús
            hMenuBrush = CreateSolidBrush(COLOR_BG);
            MENUINFO mi = { sizeof(MENUINFO) };
            mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
            mi.hbrBack = hMenuBrush;
            SetMenuInfo(hMenu, &mi);

            EnableDarkTitleBar(hwnd);
            UpdateTitle(hwnd);
            break;
        }

        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            MoveWindow(hEdit, 0, 0, rc.right, rc.bottom, TRUE);
            break;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, COLOR_TEXT);
            SetBkColor(hdc, COLOR_BG);
            return (LRESULT)hBrushBG;
        }

        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, hBrushBG);
            return 1;
        }

        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT *mis = (MEASUREITEMSTRUCT*)lParam;
            if (mis->CtlType == ODT_MENU) {
                LPCWSTR text = (LPCWSTR)mis->itemData;
                HDC hdc = GetDC(hwnd);
                SIZE sz;
                GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &sz);
                ReleaseDC(hwnd, hdc);
                mis->itemWidth = sz.cx + 24;
                mis->itemHeight = sz.cy + 10;
            }
            break;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT*)lParam;
            if (dis->CtlType == ODT_MENU) {
                LPCWSTR text = (LPCWSTR)dis->itemData;
                BOOL selected = (dis->itemState & ODS_SELECTED);
                HBRUSH hbr = CreateSolidBrush(selected ? RGB(55, 55, 55) : COLOR_BG);
                FillRect(dis->hDC, &dis->rcItem, hbr);
                DeleteObject(hbr);
                SetTextColor(dis->hDC, COLOR_TEXT);
                SetBkMode(dis->hDC, TRANSPARENT);
                RECT rc = dis->rcItem;
                rc.left += 10;
                DrawTextW(dis->hDC, text, -1, &rc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
            }
            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_FILE_NEW:   NewFile(hwnd); break;
                case ID_FILE_OPEN:  DoOpenFile(hwnd); break;
                case ID_FILE_SAVE:  SaveFile(hwnd); break;
                case ID_FILE_SAVEAS: SaveFileAs(hwnd); break;
                case ID_FILE_EXIT:  PostMessage(hwnd, WM_CLOSE, 0, 0); break;
                case ID_EDIT:
                    if (HIWORD(wParam) == EN_CHANGE) {
                        fileModified = TRUE;
                        UpdateTitle(hwnd);
                    }
                    break;
            }
            break;

        case WM_DESTROY:
            DeleteObject(hBrushBG);
            DeleteObject(hMenuBrush);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ---------- Aceleradores de teclado (Ctrl+N, Ctrl+O, Ctrl+S) ----------

HACCEL CreateAppAccelerators() {
    ACCEL accels[] = {
        { FVIRTKEY | FCONTROL, 'N', ID_FILE_NEW },
        { FVIRTKEY | FCONTROL, 'O', ID_FILE_OPEN },
        { FVIRTKEY | FCONTROL, 'S', ID_FILE_SAVE },
    };
    return CreateAcceleratorTableW(accels, 3);
}

// ---------- Punto de entrada ----------

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPWSTR args, int nShowCmd) {
    // Le decimos a Windows que nosotros manejamos el escalado por DPI,
    // así no estira/pixela la ventana en pantallas con escalado activado.
    SetProcessDPIAware();

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"BlocNotasOscuroClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(MY_BG_WINDOW);
    wc.hIcon = LoadIconW(hInstance, L"IDI_APPICON");
    if (!wc.hIcon) wc.hIcon = LoadIcon(NULL, IDI_APPLICATION); // respaldo si no se compiló el recurso

    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Bloc de Notas Oscuro",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nShowCmd);
    UpdateWindow(hwnd);

    HACCEL hAccel = CreateAppAccelerators();

    // Si Windows nos pasó una ruta de archivo como argumento (esto pasa
    // cuando esta app es la predeterminada y el usuario hace doble clic
    // en un .txt), la cargamos automáticamente al iniciar.
    if (args != NULL && args[0] != L'\0') {
        WCHAR path[MAX_PATH];
        LPCWSTR src = args;
        if (src[0] == L'"') {
            // Windows envuelve la ruta en comillas si contiene espacios
            wcsncpy(path, src + 1, MAX_PATH - 1);
            path[MAX_PATH - 1] = L'\0';
            WCHAR *endQuote = wcschr(path, L'"');
            if (endQuote) *endQuote = L'\0';
        } else {
            wcsncpy(path, src, MAX_PATH - 1);
            path[MAX_PATH - 1] = L'\0';
        }
        LoadFileIntoEditor(hwnd, path);
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (!TranslateAcceleratorW(hwnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return 0;
}
