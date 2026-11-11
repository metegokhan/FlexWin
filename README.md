# FlexWin

**English** | [Türkçe](README.tr.md)

**FlexWin** is an ultra-lightweight, portable native C++20/Win32 window manager for Windows 10/11 designed to automatically resize, reposition, unlock, minimize, close, or adjust transparency of enterprise and legacy application windows that disallow resizing.

## Features

- **Zero External Dependencies & Portable:** Compiled with static CRT (`/MT`). No .NET or VC++ Redistributable required (~880 KB total across all binaries). Stores all settings next to `FlexWin.exe` in `rules.json`.
- **Two-Layer Resize Unlock (x64 & x86):**
  - *Out-of-process:* Unlocks `WS_THICKFRAME | WS_MAXIMIZEBOX`, removes titlebar (`Borderless`), compensates Windows 10/11 invisible DWM borders (`DWMWA_EXTENDED_FRAME_BOUNDS`), forces `SetWindowPos(..., SWP_NOSENDCHANGING)` with anti-snapback guard.
  - *Hook DLL (`FlexWinHook64.dll` & `FlexWinHook32.dll` + `FlexWinHelper32.exe`):* Breaks stubborn `WM_GETMINMAXINFO`, `WM_WINDOWPOSCHANGING`, `WM_STYLECHANGING`, and optional `WM_NCHITTEST` locks on both **64-bit** and **32-bit** applications via lock-free shared memory (`SharedHookTable`).
- **Lifecycle Actions & Gradual Transparency:**
  - **Auto Minimize** and **Auto Close** (with configurable millisecond delay) for unwanted popups/nag screens.
  - **Graduated Opacity (10%–100%)** slider with optional **Smooth Fade** transition (`WS_EX_LAYERED` + `SetLayeredWindowAttributes`).
- **Event-Driven Zero-Polling Engine:** Uses `SetWinEventHook` (`EVENT_OBJECT_SHOW`..`EVENT_OBJECT_NAMECHANGE`, `EVENT_OBJECT_LOCATIONCHANGE`, `EVENT_OBJECT_DESTROY`) so even apps that set their window title asynchronously (Electron, Java, WebView2) are matched immediately.
- **Easy Rule Creation & Backup:**
  - Drag-and-drop **Crosshair Picker** with live target border highlight.
  - **Open Windows Inspector** showing process, class, size, lock state, `x64`/`x86` architecture, and Admin/UIPI privilege status.
  - **Multi-Profile Support** with **Profile Backup (Export / Import JSON)**, live **Monitor Preview** (`PerMonitorV2` DPI-aware), **System Tray** integration, and **TR / EN Bilingual UI**.
  - Optional experimental Win32 direct child control proportional scaling (`Revert` restores original size, style, and opacity).

## Default Global Hotkeys

- `Ctrl + Alt + R`: Unlock resize on the currently focused window
- `Ctrl + Alt + B`: Toggle Borderless mode on the focused window
- `Ctrl + Alt + T`: Toggle Always on Top on the focused window
- `Ctrl + Alt + A`: Apply all rules in the active profile immediately

## Building from Source (Visual Studio 2022 C++ Build Tools)

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Config Release -RunTests
```

Portable binaries will be staged into `dist/` (`FlexWin.exe`, `FlexWinHook64.dll`, `FlexWinHelper32.exe`, `FlexWinHook32.dll`).

## License

Licensed under the [MIT License](LICENSE).
