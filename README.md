# Project Altina

A toy project.

## Build

### Prerequisite

- CMake >= 3.28
- C++23 toolchain
  - Visual Studio 2022 / 2026 / Insiders
  - clang-cl

Optional dependencies by module:

- `Scripting.CoreCLR`: .NET SDK (`dotnet` in PATH)
- `Rhi.D3D11`: Windows SDK (D3D11, DXGI, D3DCompiler libs)
- `Rhi.Vulkan`: Vulkan SDK (CMake `find_package(Vulkan)` must succeed)
- `ShaderCompiler`:
  - DXC backend: DirectX Shader Compiler (dxcompiler.lib/dxcompiler.dll + `dxc.exe` in PATH)
  - Slang backend: `slangc.exe` in PATH

### Build (Windows)

Use the preset-driven scripts (recommended):

```powershell
# Build engine core
.\Scripts\BuildEngine.ps1 -Preset windows-msvc-relwithdebinfo -Target AltinaEngineCore

# Build the minimal demo
.\Scripts\BuildDemo.ps1 -Preset windows-msvc-relwithdebinfo
```

If you want to run tests after build:

```powershell
.\Scripts\BuildEngine.ps1 -Preset windows-msvc-relwithdebinfo -Target AltinaEngineTests -RunTests
```

### Build (manual CMake)

```powershell
cmake --preset windows-msvc-relwithdebinfo
cmake --build --preset windows-msvc-relwithdebinfo --target AltinaEngineCore
```
