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
│  ├─ Engine/
│  │  ├─ Core/
│  │  │  ├─ Public/              # Headers shared with other modules
│  │  │  └─ Private/             # Private headers + sources for Core
│  │  ├─ Render/
│  │  └─ ...
│  ├─ Runtime/
│  │  └─ Launch/                 # Game/application entry modules
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
- Optional: add a `Module.Build.cs`-style manifest (JSON or TOML) if you want data-driven build metadata later.
- `Engine/Core` acts as the foundation; organise its public API into subfolders such as `Types/` (aliases + concepts), `Containers/`, and `Math/` for low-level primitives.

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
- Consumers link the module target: `target_link_libraries(AltinaEngineRender PRIVATE AltinaEngineCore)`. The public include path becomes available automatically.
- Keep inter-module includes in `Public/` headers minimal; prefer forward declarations plus private includes in `.cpp` files.
- For friend modules sharing internals, use an explicit `Friend` subfolder and document the rationale.

## Demo Projects
- Each demo lives under `Demo/<DemoName>/` and may include lightweight gameplay or showcase code in its local `Source/` folder.
- Keep demo `Source/` projects thin: depend on engine modules via exported headers and link against staged binaries from the engine build.
- Route build outputs into each demo's `Binaries/` directory by configuring CMake `RUNTIME_OUTPUT_DIRECTORY` or post-build copy scripts.
- Store demo-only assets (textures, levels, configs) in `Demo/<DemoName>/Assets/`; reference shared assets from the top-level `Assets/` when reuse makes sense.
- Use per-demo `CMakeLists.txt` that consume the engine via `add_subdirectory(..\\..\\Source)` or through installed packages to mirror end-user integration.
- Engine modules post-build copy their outputs into demo `Binaries/` directories (see `Source/Engine/Core/CMakeLists.txt`).

## Build & Toolchain Strategy
- **Primary generator**: CMake (>=3.28) with preset support (`CMakePresets.json`) for Windows MSVC, ClangCL, Linux GCC/Clang, and macOS.
- **IDE integration**: configure compile commands for tooling (`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`).
- **Linkage**: engine modules build as shared libraries (`SHARED`) using `AE_CORE_API` export macros; toggle to static via `BUILD_SHARED_LIBS=OFF` as needed for testing.
- **Build orchestration**: call CMake via thin PowerShell/Bash wrappers stored in `Scripts/` for reproducible CI steps.
- **Scripts**: `Scripts/BuildEngine.ps1|.sh` and `Scripts/BuildDemo.ps1|.sh` drive preset-based builds for engine libraries and demos independently (defaulting to the `*-relwithdebinfo` presets).
- **Toolchains**: set `-DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg.cmake>` manually (or export `VCPKG_ROOT`) before configuring if you rely on vcpkg manifests.
- **CI/CD**: `.github/workflows/ci.yml` runs configure + build across Windows and Linux using the shared presets.

## Linking Strategy Comparison

### Current Approach — Dynamic Libraries (DLL/.so)
- **Pros**: share engine binaries across demos and tools; faster iteration when only engine code changes; enables runtime module reload/hot patching; clearer ABI boundary for plugin ecosystems; ship engine updates without recompiling game code.
- **Cons**: requires export macros (`AE_CORE_API`) and disciplined symbol visibility; additional packaging for platform-specific runtimes; ABI breakage risks between engine and demos; slightly heavier load-time complexity (dependency graphs, versioning).
- **Implementation notes**: `AltinaEngineCore` builds with `AE_CORE_BUILD` defined, exporting surface symbols via `CoreAPI.h`. Post-build steps stage the `.dll/.so` into demo `Binaries/` folders.

## Coding Style Guidelines
- **File layout**: mirror Unreal Engine conventions; one primary type per file named `<TypeName>.h/.cpp`, with public headers under `Public/` and implementations under `Private/`.
- **Type prefixes**:
	- Templates and `concept`s start with `T` (`TVector`, `TUniquePtr`).
	- Enums use `E` (`EColorSpace`).
	- Plain structs/classes use `F` unless they derive from a special base (`FVector3`).
	- Interfaces start with `I`;
- **Member prefixes**:
	- Constants/static constexpr values start with `k` (`kMaxLights`).
	- Global variables/g_singletons start with `g` (`gEngineConfig`).
	- Member variables use `m` (`mTransform`).
	- Pointer members append `Ptr` (`mRenderDevicePtr`) to clarify ownership semantics.
- **Functions & methods**: PascalCase (`InitializeRenderer`, `LoadModule`); boolean getters can use `Is`/`Has` prefix.
-- **Namespace rules**: wrap engine code in `AltinaEngine::` sub-namespaces per module; avoid `using namespace` in headers.
- **Includes**: order from local module headers, other engine modules, third-party libs, then STL; enforce angle brackets vs quotes per category.
- **Formatting**: 120-column soft limit, tabs for indentation in code blocks that mimic UE (or configured via clang-format profile once finalized).
- **Comments**: use `//` for brief notes, `/** */` for API docs; prefer documenting module boundaries and lifecycle contracts.
- **Header hygiene**: forward-declare where possible, include only what you use to minimize build times.
- **Automation**: `config/clang-format` and `config/clang-tidy` back editor integrations and CI checks; keep them version-controlled alongside code changes.

## Dependency Management
- Use Conan or vcpkg for third-party C/C++ dependencies; prefer locking versions via `conanfile.py` or `vcpkg.json`.
- Mirror vendored libraries into `External/` only when custom patches are required; otherwise rely on the package manager cache.
- Define platform abstraction interfaces in `Engine/Core` so dependencies remain localized.
- Include a toolchain manifest (`conanprofile` or `vcpkg-configuration.json`) to pin compilers and triplets per platform.
- Current baseline: `vcpkg.json` + `vcpkg-configuration.json` lock the builtin registry at `f38028b82d90d92d2bb2f9b2585fa1313dcd89e5`.

## Testing & Samples
- Keep engine self-tests under `Source/Tests/` with doctest, Catch2, or GoogleTest integrated as a module.
- Create minimal showcase demos in `Demo/<DemoName>` consuming the engine via CMake `FetchContent` or by linking the built modules.

## Documentation & Governance
- Maintain developer docs in `docs/` (setup guides, module rules, contribution standards).
- Keep `docs/ModuleContracts.md` current with dependency boundaries and initialization order changes.
- Reference `docs/CodingStyle.md` for detailed naming, formatting, and tooling guidance.
- Introduce a `CONTRIBUTING.md` once module conventions stabilize.

## Near-Term Focus
- Flesh out `Engine/Core` with logging, assertion, and platform abstraction services.
- Prototype `Engine/Render` to validate multi-module linking and resource lifetimes.
- Stand up `Source/Tests` with a GoogleTest or Catch2 harness running via CTest.
- Add packaging scripts under `Scripts/` to bundle demo binaries and assets for distribution.
- Draft contribution guidelines and code review checklist in `docs/`.

