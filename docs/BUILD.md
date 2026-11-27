# Build Guide

## Requirements

- Windows 10 or later (64-bit).
- Visual Studio 2022 or 2026 with the **Desktop development with C++** workload.
- CMake 3.20 or newer.
- A Windows 10/11 SDK with DirectX 11 headers and libraries.

No external package manager, vcpkg, Python or network access is needed to build.
Dear ImGui and `dkcomp` are vendored.

## Configure and build

From the repository root:

```bat
build.bat
```

or manually:

```bat
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --parallel
```

The generator name depends on the installed Visual Studio. To list the available
generators:

```bat
cmake --help
```

Common alternatives:

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

## Outputs

```
build\Release\DKEditor.exe
build\Release\DKEditorSelftest.exe
```

`DKEditor.exe` is a Windows GUI application (`WIN32` subsystem). It does not open
a console.

## Build configuration

| Setting | Value |
| --- | --- |
| C++ standard | C++20 |
| Character set | Unicode / `wWinMain` |
| Graphics | DirectX 11 |
| UI | Dear ImGui 1.91.9b |
| Compression | Kingizor `dkcomp` (C, static library) |
| Extra libraries | `d3d11`, `dxgi`, `d3dcompiler`, `comdlg32`, `shell32` |

MSVC builds use `/utf-8`, `/W3`, `/permissive-` and `/MP`.

## Debug build

```bat
cmake --build build --config Debug
```

The Debug build writes to `build\Debug\`. The editor does not require debug
DirectX layers.

## Rebuilding only the editor

```bat
cmake --build build --config Release --target DKEditor
```

## Rebuilding only the self-test

```bat
cmake --build build --config Release --target DKEditorSelftest
```

## Clean build

```bat
rmdir /s /q build
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --parallel
```

## Running from Visual Studio

Open the generated `build\DKEditor.sln`, set `DKEditor` as the startup project,
and pass a ROM path in **Debugging > Command Arguments** if desired.

## Third-party licences

- `third_party/imgui/LICENSE.txt` - Dear ImGui, MIT.
- `third_party/dkcomp/` - Kingizor `dkcomp`, MIT (see the upstream README and
  source headers).

Do not remove those files.
