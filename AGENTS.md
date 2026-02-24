# AltinaEngine Guide for Agents

## Specifications and Todo-lists 
- Reference specifications in `Docs/Prompts` folder first.

## Commonly Used Commandlines
- Building demo: `Scripts/BuildDemo.ps1` (Always run before commit)
- Run tests: `Scripts/RunTests.ps1` (Always run before commit)

## Important Rules
- Reference `Docs/CodingStyle.md` for detailed naming, formatting, and tooling guidance.
  - **MINIMALIZE** the STL use in engine runtime. 
    - **DO NOT** use `std::filesystem`.
    - **DO NOT** use stl types as function argument.
  - **DO NOT** use redundant namespaces like `AltinaEngine::Core::Container::TOwner<T>`, use `using` before.
  - **DO** use rust-like type aliases, like `i8`,`u8`,`f32`
  - **DO NOT** use platform-specific headers in engine header files, like `<windows.h>`, `<unistd.h>`
- Modernize codes, you can use C++23 features (if supported).
  - The project builds on `Visual Studio Insiders` or `Visual Studio 2026`!
  - Use C++ concepts and compile-time exprs **AS POSSIBLE AS YOU CAN**
- Unless specified, do not introduce thirdparty libraries!
- Unless specified, do not add new module dependency in CMake!
- Implement commonly-used utilities in `Core` module.**DO NOT** implement them in module-specific domain. Such utilities like filesystem operations(place them under `Core/Platform`); string and codecvt utilities(in `Core/Utility`); algorithms like sorting, prefix sum,...(place them under `Core/Algorithm`).

### Module Layout Rules
- Each module sits under `Source/<Domain>/<ModuleName>/` with `Public/` and `Private/` subfolders.
- `Public/` contains the module's exported headers organized by feature (e.g. `Public/Rendering/Pipeline.h`).
- `Private/` hosts implementation headers and all `.cpp` files. Subfolders can mirror `Public/` for clarity.
- Use `ModuleName/Public` as the include root for other modules: `#include "Core/Application/Application.h"`.
  
## Demo Projects
- Each demo lives under `Demo/<DemoName>/` and may include lightweight gameplay or showcase code in its local `Source/` folder.
- **DO** Keep demo `Source/` projects thin: depend on engine modules via exported headers and link against staged binaries from the engine build.
- **DO NOT** wire engine-level functionalities into demo domain. Such functionalities include asset loading, level streaming, commonly used rendering features(like FSR,TAAU,Bloom,VSM,Virtual Geometry,Raytracing,Work Graph,SDF,Neural rendering).
  - Propose todo or suggestion under `Docs/Todo` if you cannot achieve the goal.

## Testing & Samples
- **ALWAYS** add tests after you introduce a new feature.
  - Keep engine self-tests under `Source/Tests/` with CTest integrated as a module.
