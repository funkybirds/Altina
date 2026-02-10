# Altina Asset System Draft

This document outlines a pragmatic asset management plan for AltinaEngine. It targets a clean separation between
runtime asset loading and editor/tooling import pipelines while keeping dependencies explicit and modular.

## Goals
- Stable asset identity and references (no direct file paths in gameplay/runtime code).
- Fast, deterministic runtime loading from cooked data.
- Reproducible import/cook pipeline driven by metadata.
- Minimal, explicit dependencies between engine modules.
- Scalable to async loading and streaming.

## Non-Goals (for now)
- Full editor UI for asset authoring.
- On-the-fly format conversion at runtime.
- Automatic dependency discovery without metadata (we will record it explicitly).

---

## 1. Terminology
- **Source Asset**: Original file from DCC/tools (PNG, FBX, WAV, etc).
- **Imported Asset**: Engine-friendly intermediate format after import (validated, normalized).
- **Cooked Asset**: Platform-specific binary data used by runtime.
- **Asset Registry**: Database mapping asset identity to paths, types, dependencies, and versions.
- **Asset Handle**: Runtime reference (UUID + type) used by code.
- **Virtual Path**: User-facing path (`Engine/Textures/Wood_Albedo`) independent of filesystem.

---

## 2. Repository Layout (Assets)
Proposed:
```
Assets/                         # Shared engine assets (optional, not mandatory yet)
  Textures/
  Meshes/
  Materials/
  Audio/
  Shaders/
Demo/<DemoName>/Assets/          # Demo-specific source assets
Source/Tools/AssetPipeline/      # Import/cook tools (not runtime)
```

Guidelines:
- Source assets live in `Assets/` or `Demo/<Demo>/Assets/`.
- Cooked outputs do not live in source folders. Keep them in build output or staging folders.
- Add `.meta` files next to source assets and check them into git.

---

## 3. Identity & Metadata
Each asset has a stable UUID and a virtual path. Metadata is stored next to source assets:

`SomeTexture.png.meta`
```json
{
  "Uuid": "a9c1b3c0-6d6e-4e16-9f4d-2d0a4f2d9a0b",
  "Type": "Texture2D",
  "VirtualPath": "Engine/Textures/Wood_Albedo",
  "SourcePath": "Assets/Textures/Wood_Albedo.png",
  "Importer": "TextureImporter",
  "ImporterVersion": 1,
  "Dependencies": [],
  "Tags": ["PBR", "Wood"],
  "License": "Internal"
}
```

Notes:
- The UUID is the authoritative key; VirtualPath is a convenience lookup.
- Dependencies are explicit (materials list textures, meshes list materials, etc.).

### 3.1 Source Dependencies vs Runtime Dependencies
We distinguish dependencies needed for import/cook from runtime load:
- **SourceDependencies**: Files referenced by the source asset (e.g., `.gltf` -> `.bin` + external textures).
- **RuntimeDependencies**: Asset handles that must load for correct runtime usage (e.g., Mesh -> Material -> Texture).

SourceDependencies exist only in `.meta` (or import database) and are used to trigger reimport. RuntimeDependencies
are emitted into the Asset Registry and used by the AssetManager at runtime.

### 3.2 Sub-Assets (Single File, Multiple Assets)
Some source files contain multiple assets (GLTF/FBX). We treat each sub-asset as a first-class asset:
- Each sub-asset gets its own UUID, type, and VirtualPath.
- The source file acts as a container only; runtime does not load it directly.
- Sub-asset UUIDs should be stable (hash of source path + sub-asset name + type).

Example `.meta` (sketch):
```json
{
  "Uuid": "9f98c213-65b0-4208-b9fd-5e9f2a7f9b20",
  "Type": "Container",
  "VirtualPath": "Demo/Minimal/Meshes/Chair",
  "SourceDependencies": ["Demo/Minimal/Assets/Chair.gltf", "Demo/Minimal/Assets/Chair.bin"],
  "SubAssets": [
    { "Uuid": "b0a...", "Type": "Mesh", "Name": "Seat", "VirtualPath": "Demo/Minimal/Meshes/Chair/Seat" },
    { "Uuid": "c2f...", "Type": "Mesh", "Name": "Back", "VirtualPath": "Demo/Minimal/Meshes/Chair/Back" },
    { "Uuid": "d51...", "Type": "Material", "Name": "Wood", "VirtualPath": "Demo/Minimal/Materials/Wood" },
    { "Uuid": "e88...", "Type": "Texture2D", "Name": "Wood_Albedo", "VirtualPath": "Demo/Minimal/Textures/Wood_Albedo" }
  ]
}
```

---

## 4. Runtime Architecture (Engine/Asset)
Create `Source/Engine/Asset/` module with the following runtime-only responsibilities:

### 4.1 Core Types
- `FAssetHandle` (Uuid + EAssetType).
- `FAssetRef`/`TAssetPtr` (runtime reference wrapper).
- `EAssetType` enum for known types.

### 4.2 AssetRegistry
Provides query-only access at runtime:
- `FindByPath(StringView path) -> FAssetHandle`
- `GetDesc(FAssetHandle) -> FAssetDesc`
- `GetDependencies(FAssetHandle) -> TVector<FAssetHandle>`

Registry is built by tooling and shipped with cooked data.

### 4.3 AssetManager
Central runtime entry point:
- `Load(FAssetHandle) -> TSharedPtr<IAsset>`
- `LoadAsync(FAssetHandle) -> FAssetFuture`
- `Unload(FAssetHandle)`

Responsibilities:
- Cache and reference counting.
- Resolve dependencies via registry.
- Dispatch to type-specific `IAssetLoader`.

### 4.4 Loaders
`IAssetLoader` per asset type:
- `bool CanLoad(EAssetType type)`
- `TSharedPtr<IAsset> Load(const FAssetDesc&, IAssetStream&)`

---

## 5. Tooling Pipeline (Tools/AssetPipeline)
The import/cook pipeline lives in tooling:

### 5.1 Import Step
- Validate source asset.
- Generate or update `.meta`.
- Produce imported intermediate (optional).
  - For GLTF/FBX: enumerate sub-assets, generate stable UUIDs, and record SourceDependencies.

### 5.2 Cook Step
- Read `.meta` + source/imported data.
- Output platform-specific cooked asset binaries.
- Emit or update `AssetRegistry.bin` (or JSON for early stage).
  - Emit RuntimeDependencies for each sub-asset (Mesh -> Material -> Texture, etc.).

---

## 6. IO & Packaging
Runtime reads only cooked assets using `IFileSystem` abstraction:
- Local loose files (dev mode).
- Pak/Bundle files (shipping).

Bundling strategy (future):
- Chunk by virtual path prefix (`Engine/*`, `Demo/Minimal/*`).
- Support per-platform bundles.

---

## 7. Versioning & Compatibility
Each asset format includes:
- Asset type version (per loader).
- Importer version (per importer).
- Optional schema version for registry.

When versions change:
- Require re-import/re-cook.
- Keep compatibility paths if necessary.

---

## 8. Diagnostics
Add logging and validation:
- Missing asset warnings show VirtualPath + UUID.
- Dependency cycles detected during registry build.
- Cook validation emits a summary report.

---

## Implementation Plan (TODO)

### Phase 0: Structure & Stubs
- [ ] Add `Source/Engine/Asset/` module (Public/Private).
- [ ] Add `AE_ASSET_API` export macro in `AssetAPI.h`.
- [ ] Wire CMake target `AltinaEngineAsset` and link to `AltinaEngineCore`.

### Phase 1: Runtime Core
- [ ] Define `FAssetHandle`, `EAssetType`, `FAssetDesc`.
- [ ] Implement `AssetRegistry` (JSON reader for bootstrap).
- [ ] Implement `AssetManager` with simple cache + sync load.

### Phase 2: Tooling MVP
- [ ] Add `Tools/AssetPipeline` skeleton (import + cook commands).
- [ ] Implement `.meta` generation for textures and meshes.
- [ ] Emit `AssetRegistry.json` for runtime load.

### Phase 3: Asset Types
- [ ] Texture2D loader (DDS/BCn or engine format).
- [ ] Mesh loader (engine format).
- [ ] Material loader (references textures).
- [ ] Audio loader (streaming-friendly format).

### Phase 4: Async Loading + Hot Reload
- [ ] Async load queue + IO thread.
- [ ] Main-thread finalize phase for GPU resources.
- [ ] Editor/dev hot reload based on `.meta` timestamp.

### Phase 5: Packaging
- [ ] Asset bundle/pak format with index table.
- [ ] Bundle per demo and per platform.
- [ ] Registry moved to binary format.

---

## Open Questions
- Preferred cooked texture format on Windows (DDS/BCn vs custom)?
- Do we need a shared global asset cache across demos?
- Should registry be JSON long-term or migrate to binary/SQLite?
