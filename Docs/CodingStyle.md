# AltinaEngine Coding Style

AltinaEngine follows Unreal Engine inspired naming and layout conventions. Keep the rules below in sync with the `config/clang-format` and `config/clang-tidy` profiles to ensure automated tooling matches human expectations.

## Naming

- **Templates & Concepts**: Prefix with `T` (e.g., `TVector`, `TOptional`).
- **Enums**: Prefix with `E` (e.g., `EColorSpace`). Enumerators use `CamelCase`.
- **Structs & Plain Classes**: Prefix with `F` (e.g., `FTransform`). And interfaces use `I`.
- **Type Aliases**: Prefix with `T` and keep `CamelCase` (e.g., `TSeconds`).
- **Globals**: Prefix variables with `g` (`gEngineConfig`).
- **Constants / constexpr**: Prefix with `k` (`kDefaultWindowWidth`).
- **Member Variables**: Prefix with `m` (`mRenderDevicePtr`). Append `Ptr` or `Ref` to clarify ownership when valuable.
- **Functions & Methods**: Use `PascalCase`. Boolean accessors begin with `Is`, `Has`, or `Should`.
- **Namespaces**: `CamelCase` under the root `AltinaEngine` namespace (e.g., `AltinaEngine::Rendering`).

## Files & Includes

- One primary type per file. Name files after the type (e.g., `FApplication.h`).
- Headers live under `Public/` when shared; implementation headers and sources remain in `Private/`.
- Include order: module headers, other engine headers, third-party headers, then STL/standard headers.
- Avoid `using namespace` in headers. Prefer explicit qualifiers or alias within implementation files.

## Formatting

- 120-character column limit; break lines proactively when readability suffers.
- Tabs for indentation; spaces for alignment.
- Attach opening braces to control statements (`if`, `for`, `class`) in UE style.
- Keep access specifiers (`public`, `private`) indented one level.
- Document public APIs with `/** ... */` blocks when behaviour is non-trivial.

## Documentation Comments

- Use Doxygen-style block comments (`/** ... */`) for public APIs in `Public/` headers.
- Keep comments concise: describe ownership, complexity guarantees (O(1), amortized O(1)), and any pre/post conditions.
- Place an example usage snippet for non-trivial containers or utilities (e.g., `TDeque`) near the type's top-level comment.
- Avoid duplicating implementation details; focus on the contract and observable behaviour.

## DLL Export Macros

- Module public headers expose an `<Module>API.h` file that defines `<MODULE>_API` (e.g., `AE_CORE_API`).
- Annotate every type or function that forms part of the module ABI with the macro so Windows builds emit symbols correctly.
- Keep implementation-only headers in `Private/` free of export macros; prefer forward declarations in `Public/` to minimise exported surface area.

## Tooling

- Run `clang-format` using the profile at `config/clang-format` before committing.
- Enable `clang-tidy` with the configuration at `config/clang-tidy`; it enforces prefix rules via `readability-identifier-naming`.
- Prefer additional static analysis (sanitizers, MSVC /analyze) in CI for safety-critical modules.
