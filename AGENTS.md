# AltinaEngine Architecture Guide

## Objectives

- Support modular engine development with clear, enforceable dependencies.
- Keep per-module source and headers collocated while preserving ergonomic includes.
- Make cross-platform builds easy to configure and automate.

## Repository Layout

```
AltinaEngine/
├─ CMakeLists.txt                # Root build entry point
├─ Source/
│  ├─ Runtime/
│  │  ├─ Core/
│  │  │  ├─ Public/              # Headers shared with other modules
│  │  │  └─ Private/             # Private headers + sources for Core
│  │  ├─ Render/
│  │  ├─ Launch/                 # Game/application entry modules
│  │  └─ ...
│  ├─ Tools/
│  │  └─ Editor/
│  └─ Tests/
├─ External/                     # Third-party libraries managed via package manager
├─ Scripts/                      # Build, packaging, automation scripts
├─ Assets/                       # Engine-wide shared assets or reference data
├─ Demo/
│  └─ <DemoName>/
│     ├─ Assets/                 # Demo-specific assets packaged with the build
│     ├─ Binaries/               # Staged runtime output (exe + dependent DLLs)
│     └─ Source/                 # Optional glue/gameplay code unique to the demo
└─ docs/
```

### Module Layout Rules

- Each module sits under `Source/<Domain>/<ModuleName>/` with `Public/` and `Private/` subfolders.
- `Public/` contains the module's exported headers organized by feature (e.g. `Public/Rendering/Pipeline.h`).
- `Private/` hosts implementation headers and all `.cpp` files. Subfolders can mirror `Public/` for clarity.
- Use `ModuleName/Public` as the include root for other modules: `#include "Core/Application/Application.h"`.
- Limit friend includes via CMake `target_link_libraries` to express allowed module dependencies.
- `Runtime/Core` acts as the foundation; organise its public API into subfolders such as `Types/` (aliases + concepts),
  `Containers/`, and `Math/` for low-level primitives.

### Cross-Module Include Strategy

- Configure each module as a CMake target:
  ```cmake
  add_library(AltinaEngineCore SHARED)
  target_sources(AltinaEngineCore
      PRIVATE
          Private/Application/Application.cpp
      PUBLIC
          FILE_SET public_headers TYPE HEADERS BASE_DIRS Public FILES
              Public/Application/Application.h
  )
  target_include_directories(AltinaEngineCore
      PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Public>
      PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private
  )
  target_compile_definitions(AltinaEngineCore PRIVATE AE_CORE_BUILD)
  ```
- Consumers link the module target: `target_link_libraries(AltinaEngineRender PRIVATE AltinaEngineCore)`. The public
  include path becomes available automatically.
- Keep inter-module includes in `Public/` headers minimal; prefer forward declarations plus private includes in `.cpp`
  files.
- For friend modules sharing internals, use an explicit `Friend` subfolder and document the rationale.

## Demo Projects

- Each demo lives under `Demo/<DemoName>/` and may include lightweight gameplay or showcase code in its local `Source/`
  folder.
- Keep demo `Source/` projects thin: depend on engine modules via exported headers and link against staged binaries from
  the engine build.
- Route build outputs into each demo's `Binaries/` directory by configuring CMake `RUNTIME_OUTPUT_DIRECTORY` or
  post-build copy scripts.
- Store demo-only assets (textures, levels, configs) in `Demo/<DemoName>/Assets/`; reference shared assets from the
  top-level `Assets/` when reuse makes sense.
- Use per-demo `CMakeLists.txt` that consume the engine via `add_subdirectory(..\\..\\Source)` or through installed
  packages to mirror end-user integration.
- Engine modules post-build copy their outputs into demo `Binaries/` directories (see
  `Source/Runtime/Core/CMakeLists.txt`).

## Build & Toolchain Strategy

- **Primary generator**: CMake (>=3.28) with preset support (`CMakePresets.json`) for Windows MSVC, ClangCL, Linux
  GCC/Clang, and macOS.
- **IDE integration**: configure compile commands for tooling (`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`).
- **Linkage**: engine modules build as shared libraries (`SHARED`) using `AE_CORE_API` export macros; toggle to static
  via `BUILD_SHARED_LIBS=OFF` as needed for testing.
- **Build orchestration**: call CMake via thin PowerShell/Bash wrappers stored in `Scripts/` for reproducible CI steps.
- **Scripts**: `Scripts/BuildEngine.ps1|.sh` and `Scripts/BuildDemo.ps1|.sh` drive preset-based builds for engine
  libraries and demos independently (defaulting to the `*-relwithdebinfo` presets).

## Linking Strategy

- `AltinaEngineCore` builds with `AE_CORE_BUILD` defined, exporting surface symbols via
  `CoreAPI.h`. Post-build steps stage the `.dll/.so` into demo `Binaries/` folders.

## Coding Style Guidelines

- Reference `Docs/CodingStyle.md` for detailed naming, formatting, and tooling guidance.

## Testing & Samples

- Keep engine self-tests under `Source/Tests/` with CTest integrated as a module.

## Documentation & Governance

- Maintain developer docs in `docs/` (setup guides, module rules, contribution standards).
- Keep `docs/ModuleContracts.md` current with dependency boundaries and initialization order changes.
- Reference `docs/CodingStyle.md` for detailed naming, formatting, and tooling guidance.
- Introduce a `CONTRIBUTING.md` once module conventions stabilize.
