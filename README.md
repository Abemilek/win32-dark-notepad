# Dark Notepad (Win32)

A minimal, dark-mode text editor for Windows — built the same way the original Windows Notepad was: **plain C and the native Win32 API**, no frameworks, no runtime dependencies beyond what Windows already ships with.

The goal: something lighter, faster to open, and easier on the eyes than the stock Notepad, without sacrificing simplicity.

## Why

Most "lightweight" text editors today are Electron apps that ship an entire Chromium runtime to open a `.txt` file. This project goes the other direction: a native Win32 window, a native `EDIT` control, and manual dark-mode theming — nothing else.

## Features

- Dark title bar and dark editor background (via `DwmSetWindowAttribute`)
- Custom dark-themed menu (owner-drawn, no default white menu bar)
- Open / Save / Save As with UTF-8 support
- Word wrap
- Opens files passed as a command-line argument (works when set as the default `.txt` handler)
- DPI-aware rendering (crisp text on scaled displays)
- Keyboard shortcuts: `Ctrl+N`, `Ctrl+O`, `Ctrl+S`
- Single `.exe`, no installer, no external DLLs beyond standard Windows system libraries
- Typical compiled size: 40–80 KB

## Requirements

- Windows 10 (1809+) or Windows 11 — earlier versions run the app but without the dark title bar
- [MinGW-w64](https://winlibs.com) (GCC for Windows) to build from source

## Building

Clone the repo, then from the project folder:

```bash
windres app_icon.rc -O coff -o app_icon.res
gcc notepad_dark.c app_icon.res -o notepad_dark.exe -mwindows -municode -ldwmapi -lcomdlg32 -O2 -s
```

Flags explained:

| Flag | Purpose |
|---|---|
| `-mwindows` | Build as a GUI app (no console window) |
| `-municode` | Use the Unicode entry point (`wWinMain`) |
| `-ldwmapi` | Needed for dark title bar support |
| `-lcomdlg32` | Needed for the Open/Save file dialogs |
| `-O2` | Optimize for speed |
| `-s` | Strip debug symbols (smaller binary) |

## Usage

| Shortcut | Action |
|---|---|
| `Ctrl+N` | New file |
| `Ctrl+O` | Open file |
| `Ctrl+S` | Save |

You can also set `notepad_dark.exe` as the default app for `.txt` files (right-click a `.txt` → *Open with* → *Choose another app* → select it → check "Always use this app").

## How the dark mode works

- The editor's background/text colors are painted manually by intercepting `WM_CTLCOLOREDIT`.
- The dark title bar comes from the DWM API (`DwmSetWindowAttribute`, available since Windows 10 1809).
- The menu bar and dropdown are owner-drawn (`WM_MEASUREITEM` / `WM_DRAWITEM`) since Windows doesn't support recoloring the default menu through documented APIs.

No third-party UI libraries or theming frameworks are used — everything above is done with the standard Win32 API.

## Ideas for contributions

- Find & Replace dialog
- Word/character count in a status bar
- Line-ending detection (CRLF/LF) and conversion
- Remember window size/position between sessions
- Zoom in/out (font scaling)

## License

MIT — see [LICENSE](LICENSE).
