# Screen Resizer

**Screen Resizer** is a Windows desktop prototype written in C++/Win32 + Direct3D 11. It currently captures the desktop via DXGI output duplication and presents it in a full-screen topmost window.

## Prerequisites

- **Operating system:** Windows 10 or Windows 11 (64-bit).
- **Build tools (option A):** Visual Studio 2022 (or Build Tools 2022) with the **Desktop development with C++** workload installed.
- **Build tools (option B):** Ninja + MSVC toolchain available from a Developer Command Prompt.
- **CMake:** 3.10 or newer (`cmake --version`).
- **DirectX runtime/SDK assumptions:**
  - Uses Direct3D 11 and DXGI APIs provided by the Windows SDK / system runtime.
  - No separate legacy DirectX SDK install is required.

## Configure and build

Run these commands from the repository root.

### Option 1: CMake + Visual Studio / MSBuild

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### Option 1A: CMake presets (recommended for repeatable Windows checks)

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug --config Debug
ctest --preset windows-debug -C Debug
```

### Option 2: CMake + Ninja

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run

After a successful build, launch the executable:

- **Visual Studio generator:** `build\Release\ScreenResizer.exe`
- **Ninja generator:** `build\ScreenResizer.exe`

You can close the prototype by pressing **Esc**.

## Current status

This project is an **early prototype**.

### Implemented

- Win32 application entry point with a full-screen topmost window.
- Direct3D 11 device initialization.
- DXGI desktop duplication capture.
- Per-frame copy/present loop to display captured desktop content.

### Planned

- Real viewport scaling/zoom behavior.
- Adjustable offsets/panning controls.
- Resolution presets and custom resolution workflows.
- Improved interaction model and production-grade error handling.

## Contributing

Contributions are welcome. Please open an issue or pull request with a clear description of the change.

## License

This project is licensed under the [MIT License](LICENSE).


## Configuration

The app now persists settings to `%LocalAppData%/ScreenResizer/settings.json` and restores them at startup.
Press `S` while the app is focused to open the Settings window for viewport/profile/startup options.

## Windows Verification

Run this from **Developer PowerShell for VS 2022** at the repo root:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\verify-windows.ps1
```

Optional:

- clean build dir first: `-Clean`
- release validation: `-Configuration Release`

Example:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\verify-windows.ps1 -Clean -Configuration Release
```
