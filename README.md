# Project Altina

A toy project.

## Build (Windows)

### Prerequisites

- CMake >= 3.28
- A C++23-capable toolchain
  - Visual Studio 2022+ (recommended: Insiders) with LLVM Toolchain
  - `clang-cl` (optional)
- Windows SDK (D3D11 / DXGI / D3DCompiler libs) for the D3D11 backend
- Vulkan SDK >= 1.4 (CMake `find_package(Vulkan)` must succeed)

> [!IMPORTANT]
> `LLVM Toolchain` is required for reflection data generation.
> 
> For IDE integration, only `Visual Studio Insiders` is tested. At minimum, your compiler should support `C++23`, with extra type-trait intrinsics available in MSVC/Clang/GCC.
>
> For DX11 shader compilation and shader reflection data generation, the Slang compiler shipped with the Vulkan SDK is used. If you do not have Vulkan SDK >= 1.4 installed, ensure `slangc` is in your PATH.

### Tooling / Dependencies

- `Scripting.CoreCLR`: .NET SDK (`dotnet` in PATH)
- `Rhi.D3D11`: Windows SDK (D3D11 / DXGI / D3DCompiler libs)
- `Rhi.Vulkan`: Vulkan SDK (CMake `find_package(Vulkan)` must succeed)
- `ShaderCompiler`:
  - DXC backend: DirectX Shader Compiler (`dxcompiler.lib` / `dxcompiler.dll`, plus `dxc.exe` in PATH)
  - Slang backend: `slangc.exe` in PATH

### Build Commands

```powershell
cmake --preset windows-msvc-relwithdebinfo
cmake --build --preset windows-msvc-relwithdebinfo --target AltinaEngineDemoMinimal
```

## Acknowledgements

### Code References

- This repository contains AI-assisted code fragments.
- This repository reuses some code from my project [Ifrit-v2](https://github.com/Aeroraven/Ifrit-v2/).

### Third-party Libraries

`AltinaEngine/Runtime` and its shipped demos only rely on system-level and runtime libraries.

`AltinaEngine/Tool` has the following third-party dependencies:

- `Assimp`: for model loading
- `TinyEXR`: for `EXR` file processing
- `TinyUSDZ`: for `USDZ` model loading
