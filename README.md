# Dark Notepad

A small, native Windows text editor written in C with the Win32 API. It brings a dark interface to a straightforward Notepad-style workflow without embedding Chromium, Electron, or another application runtime. The goal is a fast launch, a small executable, and a focused editing experience.

## Features

- Native Win32 window and multiline edit control
- Dark editor, menu, and title bar where supported by Windows
- Open, Save, and Save As dialogs
- UTF-8 (with or without a BOM) and UTF-16 LE/BE file reading; files saved as UTF-8 without a BOM
- System ANSI code-page fallback for text that is not valid UTF-8
- Unsaved-change prompts before New, Open, and Exit
- Visible error messages for file and conversion failures
- Files up to 16 MiB, with the editor text limit set explicitly
- `Ctrl+N`, `Ctrl+O`, and `Ctrl+S` shortcuts
- Optional path argument for opening a file from Explorer or the command line
- No third-party runtime dependencies

## Scope and limitations

This is a minimal Notepad-inspired editor, not a feature-for-feature replacement for current Windows Notepad. It intentionally keeps the interface simple and avoids animations and large frameworks. It currently supports one document at a time, does not preserve the input file's original encoding or byte-order mark, and saves text as UTF-8. The 16 MiB file limit keeps memory use predictable for this small editor. The standard Win32 edit control is used, so very large documents and advanced editor features are outside the current scope.

The editor treats files as text. Binary files and text containing embedded NUL characters are not supported. ANSI fallback depends on the Windows system code page and is intended for legacy text files, not as a universal encoding detector. The dark title bar depends on Windows version and system support; the editor and menu colors are painted by the application.

## Requirements

- Windows 10 or Windows 11
- MinGW-w64 GCC and `windres` to build locally

## Build

From the repository directory in a MinGW-w64 shell:

```sh
windres app_icon.rc -O coff -o app_icon.res
gcc notepad_dark.c app_icon.res -o dark-notepad.exe -mwindows -municode -ldwmapi -lcomdlg32 -O2 -s
```

`-mwindows` creates a GUI executable, `-municode` selects `wWinMain`, and the two libraries provide DWM attributes and common file dialogs. The executable uses only Windows system libraries.

## Use

Run `dark-notepad.exe`, open a file through **File → Open**, or pass a file path:

```sh
dark-notepad.exe "C:\path with spaces\notes.txt"
```

You can also select the executable as the default app for `.txt` files in Windows.

## Continuous integration and releases

GitHub Actions builds the application on pushes and pull requests. Pushing a version tag such as `v1.1.0` builds the executable and publishes a GitHub release using the matching file in `release-notes/`. The release workflow can also be started manually from the Actions tab. See [`.github/workflows/ci.yml`](.github/workflows/ci.yml) and [`.github/workflows/release.yml`](.github/workflows/release.yml).

## License

MIT. See [LICENSE](LICENSE).
