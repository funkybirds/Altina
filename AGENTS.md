# AltinaEngine Guide for Agents

## Specifications and Todo-lists 
- **DO** reference specifications in `Docs/Spec` folder first.

## Module Layout Rules
- Each module sits under `Source/<Domain>/<ModuleName>/` with `Public/` and `Private/` subfolders.
- `Public/` contains the module's exported headers organized by feature (e.g. `Public/Rendering/Pipeline.h`).
- `Private/` hosts implementation headers and all `.cpp` files. Subfolders can mirror `Public/` for clarity.
- Use `ModuleName/Public` as the include root for other modules: `#include "Core/Application/Application.h"`.

## Structure
Some core modules:
- `Runtime`: The runtime of `Project Altina`
  - `Rhi.General`: Encapsulation of different graphics API backends. It should be **stateless**. 
  - `RenderCore`: Basic rendering infrastructures, like `RenderGraph`, common render resources and definitions, scene batching and reorganization, etc.
  - `Rendering`: Renderers, rendering asset processing, and common render passes.

### Notes on `Rhi`
- **DO NOT** keep states tracked for resources (except for `RefCount`). **DO** keep them **STATELESS**.
  - If you want to implement such functionality, switch to `RenderCore` or `Rendering`.

### Notes on `RenderCore`
- General computing infrastructure is also acceptable in this module (like `WorkGraph` infrastructures). But only provide infrastructure-level utils.
  - Do not introduce operation-specific-implementations here, like neural layers(`Conv2D`,`Linear`,`SelfAttn`,...) and ops(`Scan`,`Reduce`,`Gather`,...)
- Do not introduce geometry processing utilities (like `Meshlet Generation`, `Mesh Simplification`, `SDF Baking`) here. But new geometry format is allowed (for neural reprenstations like `3DGS`). Also, resource proxy (like `Compute-Shader-Generated Vertices` or `Procedural Mesh`) is also allowed.
- Unless specified, do not introduce effects (like `Dithering`, `TAAU`, `DLSS`)、techniques(like `Clustered Lighting`,`DDGI`,`Virtual Geometry`) and asset processing utilities here!
  - Exception is that it reorganizes data. For example, `Virtual Geometry` might extend data section for static meshes.

### Notes on `Asset`
- **DO NOT** directly implement a `Loader` that relies on a external format (like `*.usdz`,`*.slang`,`*.exr`,`*.glb`,`*.blend`,`*.mp3`,etc)
- **DO** implement a `Importer` in `Tool/AssetPipe` to convert it into internal format.
  - **DO** specify external asset dependencies when introduces a new importer (like `*.gltf`)

## Building
You can use CMake (with reldebinfo preset), if it fails, use following shortcuts:
- Building demo: `Scripts/BuildDemo.ps1` (Always run before commit)
- Run tests: `Scripts/RunTests.ps1` (Always run before commit)

- **NOT RECOMMENDED** to use `Debug` profile. You might encounter linking errors.

## Coding Styles
- **DO** reference `Docs/CodingStyle.md` for detailed naming, formatting, and tooling guidance.
- **DO NOT** introduce non-English characters in any source files.

### Important Coding Rules
- **DO** use modern C++ features. You can use `C++23` standard if the compiler supports.
  - The project builds on `Visual Studio Insiders` or `Visual Studio 2026`!
- **MINIMALIZE** the STL use in engine runtime. 
  - **DO NOT** use `std::filesystem`. Instead, use `FPath`.
  - **DO NOT** use stl types as function argument.
  - **DO NOT** use `std::shared_ptr<T>`,`std::unique_ptr<T>`. Instead, use `TShared<T>` and `TOwner<T>`.
  - **DO NOT** use `std::string` and `std::string_view`. Instead, use `FString` for strings, `FNativeString` for ascii strings, and `TChar*` for cstring.
  - **DO NOT** use `<type_traits>`. Instead, use concepts and templates defined in `Core/Types`
- **DO NOT** use redundant namespaces like `AltinaEngine::Core::Container::TOwner<T>`, use `using` before.
- **DO** use rust-like type aliases, like `i8`,`u8`,`f32`
- **DO NOT** use platform-specific headers in engine header files, like `<windows.h>`, `<unistd.h>`
- **DO** use concepts and `requires` expression.
  - If possible, **DO NOT** use `static_assert` and `enable_if`

### Functionality Implementation Rules
- **DO** implement commonly-used utilities in `Core` module. **DO NOT** implement them in module-specific domain. **ESPECIALLY DO NOT** place them in module-specific headers.
  - Such utilities like filesystem operations(place them under `Core/Platform`); string and codecvt utilities(in `Core/Utility`); algorithms like sorting, prefix sum, graph theory,...(place them under `Core/Algorithm`); Geometric and linear algebra utilities, spherical harmonics, integrals,... (place them under `Core/Math`)
- **DO NOT** use platform-specific headers in engine header files, like `<windows.h>`, `<unistd.h>`.
  - You can **ONLY** use these functions in `Core/Platform`, `Rhi.D3D11`, `Rhi.D3D12`(if presents) and `Rhi.Metal`(if presents). Otherwise, lookup `Core/Platform` first!

## Libraries and Dependencies
- Thirdparty libraries are located in `ThirdParty` submodule.
- Unless specified, **DO NOT** introduce thirdparty libraries!
- Unless specified, **DO NOT** add new module dependency in CMake!

## Demo Projects
- Each demo lives under `Demo/<DemoName>/` and may include lightweight gameplay or showcase code in its local `Source/` folder.
- **DO** Keep demo `Source/` projects thin: depend on engine modules via exported headers and link against staged binaries from the engine build.
- **DO NOT** wire engine-level functionalities into demo domain. Such functionalities include asset loading, level streaming, commonly used rendering features(like FSR,TAAU,Bloom,VSM,Virtual Geometry,Raytracing,Work Graph,SDF,Neural rendering).
  - Propose todo or suggestion under `Docs/Todo` if you cannot achieve the goal.

## Testing & Samples
- **ALWAYS** add tests after you introduce a new feature.
  - Keep engine self-tests under `Source/Tests/` with CTest integrated as a module.
