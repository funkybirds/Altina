# AltinaEngine Guide for Agents

Project Altina is a `personal` playground (or toy gaming engine) to validate gaming techniques and programming ideas. Architecture, runtime performance and visual effects are top priorities.

## Definitions
- **Rules** are things that you MUST follow, whenever and whatever, without no exceptions. You cannot violate a **Rule**. 
- **Guidelines** are things that you should follow in almost all cases. If there are users' prompts and specifications, follow users' requirement first.
- **Suggestions** are things that you are suggested to follow. You should make architectural considerations before you accept a suggestion.

## Specifications and Prototyping
- **Rule**: Do reference specifications in `Docs/Spec` folder first.
- **Suggestion**: Prototype an idea with `python`. Call `python3` or `py` on Windows.
  - **Guideline**: Keep prototypes in `**/Misc/` folder and not tracked by `git`.
  
## Module Layout
- Each module sits under `Source/<Domain>/<ModuleName>/` with `Public/` and `Private/` subfolders.
- `Public/` contains the module's exported headers organized by feature (e.g. `Public/Rendering/Pipeline.h`).
- `Private/` hosts implementation headers and all `.cpp` files. Subfolders can mirror `Public/` for clarity.
- Use `ModuleName/Public` as the include root for other modules: `#include "Core/Application/Application.h"`.

## Libraries and Dependencies
- Thirdparty libraries are located in `ThirdParty` submodule.
- **Rule**: Unless specified, **DO NOT** introduce thirdparty libraries!
- **Rule**: Unless specified, **DO NOT** add new module dependency in CMake!
- **Suggestion**: This is an experimental and personal project, so you can make these hand-crafted without third-party dependencies.

## Structure
Some core modules:
- `Runtime`: The runtime of `Project Altina`
  - `Rhi.General`: Encapsulation of different graphics API backends. It should be **stateless**. 
  - `RenderCore`: Basic rendering infrastructures, like `RenderGraph`, common render resources and definitions, scene batching and reorganization, etc.
  - `Rendering`: Renderers, rendering asset processing, and common render passes.

### Notes on `Rhi`
- **Rule**: Do not keep states tracked for resources (except for `RefCount`). Do keep them stateless.
- **Suggestion**: If you want to implement such functionality, switch to `RenderCore` or `Rendering`.

### Notes on `RenderCore`
- **Rule**: API-specific and low-level infrastructures should be implemented in `Rhi` layer, not here! (like `WorkGraph Shader` classes,`Cooperative Vector` support)
- **Guideline**: General computing infrastructure is also acceptable in this module (like `WorkGraph` infrastructures). But only provide infrastructure-level utils.
  - **Rule**: Do not introduce operation-specific-implementations here, like neural layers(`Conv2D`,`Linear`,`SelfAttn`,...) and ops(`Scan`,`Reduce`,`Gather`,...)
- **Rule**: Do not introduce geometry processing utilities (like `Meshlet Generation`, `Mesh Simplification`, `SDF Baking`) here. But new geometry format is allowed (for neural reprenstations like `3DGS`). Also, resource proxy (like `Compute-Shader-Generated Vertices` or `Procedural Mesh`) is also allowed.
- **Rule**: Unless specified, do not introduce effects (like `Dithering`, `TAAU`, `DLSS`)、techniques(like `Clustered Lighting`,`DDGI`,`Virtual Geometry`) and asset processing utilities here!
  - **Suggestion**: Exception is that it reorganizes data. For example, `Virtual Geometry` might extend data section for static meshes.

### Notes on `Asset`
- **Rule**: Do not directly implement a `Loader` that relies on a external format (like `*.usdz`,`*.slang`,`*.exr`,`*.glb`,`*.blend`,`*.mp3`,etc)
- **Suggstion**: Implement a `Importer` in `Tool/AssetPipe` to convert it into internal format. Specify external asset dependencies when introduces a new importer (like `*.gltf`)

## Building
- **Suggstion**: You can use CMake (with reldebinfo preset), if it fails, use following shortcuts:
  - Building demo: `Scripts/BuildDemo.ps1` (Always run before commit)
  - Run tests: `Scripts/RunTests.ps1` (Always run before commit)
- **Suggstion**: Do not use `Debug` profile. You might encounter linking errors.

## Coding Styles
- **Rule**: Reference `Docs/CodingStyle.md` for detailed naming, formatting, and tooling guidance.
- **Rule**: Do not introduce non-English characters in any source files.

### Assertion, Logging and Error Handling
- **Rule**: Do log internal processes with **PROPER CATEGORY**.
  - **Guideline**: Do not log **FREQUENTLY** on a performance-aware path, like `Tick()`.
- **Guideline**: Do use runtime assertions in need.
  - **Guideline**: Do use `Assert` for critical checks, and `DebugAssert` for debugging and performance purpose.
  - **Guideline**: Not recommended to use `if(!some_condition){return;}` for workground. Instead, use runtime assertions.
  - **Rule**: Must not use `try` experssion.
- **Rule**: Do not use C++ exceptions

### Important Coding Rules
- **Guideline**: Use modern C++ features. You can use `C++23` standard if the compiler supports.
  - The project builds on `Visual Studio Insiders` or `Visual Studio 2026`!
- **Guideline**: Minimalize the STL use in engine runtime. 
  - **Rule**: **DO NOT** use `std::filesystem`. Instead, use `FPath`.
  - **Rule**: **DO NOT** use stl types as function argument.
  - **Rule**: **DO NOT** use `std::shared_ptr<T>`,`std::unique_ptr<T>`. Instead, use `TShared<T>` and `TOwner<T>`.
  - **Rule**: **DO NOT** use `std::string` and `std::string_view`. Instead, use `FString` for strings, `FNativeString` for ascii strings, and `TChar*` for cstring.
  - **Rule**: **DO NOT** use `<type_traits>`. Instead, use concepts and templates defined in `Core/Types`
- **Rule**: **DO NOT** use redundant namespaces like `AltinaEngine::Core::Container::TOwner<T>`, use `using` before. This applies for all type traits, concepts and containers.
- **Rule**: **DO** use rust-like type aliases, like `i8`,`u8`,`f32`
- **Rule**: **DO NOT** use platform-specific headers in engine header files, like `<windows.h>`, `<unistd.h>`
- **Rule**: **DO** use concepts and `requires` expression.
  - **Rule**: If possible, **DO NOT** use `static_assert` and `enable_if`

### Functionality Implementation
- **Rule**: **DO** implement commonly-used utilities in `Core` module. **DO NOT** implement them in module-specific domain. **MUST NOT** place them in module-specific headers.
  - Such utilities like filesystem operations(place them under `Core/Platform`); string and codecvt utilities(in `Core/Utility`); algorithms like sorting, prefix sum, graph theory,...(place them under `Core/Algorithm`); Geometric and linear algebra utilities, spherical harmonics, integrals,... (place them under `Core/Math`)
- **Rule**: **MUST NOT** use platform-specific headers in engine header files, like `<windows.h>`, `<unistd.h>`.
  - You can **ONLY** use these functions in `Core/Platform`, `Rhi.D3D11`, `Rhi.D3D12`(if presents) and `Rhi.Metal`(if presents). Otherwise, lookup `Core/Platform` first!

## Demo Projects
- Each demo lives under `Demo/<DemoName>/` and may include lightweight gameplay or showcase code in its local `Source/` folder.
- **Guideline**: **DO** Keep demo `Source/` projects thin: depend on engine modules via exported headers and link against staged binaries from the engine build.
- **Rule**: **DO NOT** wire engine-level functionalities into demo domain. Such functionalities include asset loading, level streaming, commonly used rendering features(like FSR,TAAU,Bloom,VSM,Virtual Geometry,Raytracing,Work Graph,SDF,Neural rendering).
  - Propose todo or suggestion under `Docs/Todo` if you cannot achieve the goal.

## Testing & Samples
- **Rule**: **ALWAYS** add tests after you introduce a new feature.
  - Keep engine self-tests under `Source/Tests/` with CTest integrated as a module.

## Miscellaneous
- This repository inherits my previous toy project `https://github.com/aeroraven/ifrit-v2`. 