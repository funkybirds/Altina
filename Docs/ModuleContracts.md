# Module Contracts

The AltinaEngine project is organized into self-contained modules. Each module declares its dependencies explicitly in CMake to keep compile times predictable and avoid accidental coupling.

## Dependency Tiers

1. **Core Tier** – Foundational services (`Runtime/Core`, `Runtime/Platform`). These modules do not depend on higher tiers and should remain free of rendering or gameplay knowledge.
2. **Systems Tier** – Subsystems that build on the core (e.g., `Runtime/Render`, `Runtime/Audio`, `Runtime/Input`). These may depend on Core but not on Launch or Demo modules.
3. **Runtime Tier** – Launchers and gameplay glue under `Runtime/Launch`. They depend on systems as needed but never the reverse.
4. **Demo Tier** – Projects in `Demo/` that showcase engine features. Demos may depend on any runtime or engine module but must not export code back into the engine.

## Rules of Engagement

- **Explicit Links**: Every dependency must be declared via `target_link_libraries`. Relying on transitive includes is prohibited.
- **Public vs Private**: Use `PUBLIC` links when symbols are part of the module contract; otherwise prefer `PRIVATE` links to keep downstream modules clean.
- **Forward Declarations**: Favour forward declarations in public headers to minimize compile-time dependencies.
- **Friend Access**: If two modules require intimate sharing, document the rationale in this file and create a dedicated `Friend` subfolder guarded by `#ifdef AE_FRIEND_MODULE` macros.
- **Runtime Binaries**: Only demo and runtime modules produce executables. Engine modules should stay as static or shared libraries.

## Initialization Order

1. Core modules (logging, platform abstraction) initialize first.
2. Systems register and initialize after the core is ready.
3. Runtime modules configure gameplay flow.
4. Demos launch the runtime modules and own asset lifetimes.

## Future Extensions

- Introduce a manifest (`Module.toml`) per module for data-driven dependency declarations.
- Validate dependency graph automatically via a CMake script or a custom linter before builds.
