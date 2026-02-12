# Altina Asset System Draft

This document outlines a pragmatic asset management plan for AltinaEngine. It targets a clean separation between
runtime asset loading and editor/tooling import pipelines while keeping dependencies explicit and modular.

## Goals
- Stable asset identity and references (no direct file paths in gameplay/runtime code).
- Fast, deterministic runtime loading from cooked data.
- Reproducible import/cook pipeline driven by metadata.
- Minimal, explicit dependencies between engine modules.
- Scalable to async loading and streaming.
- Deterministic, cache-friendly build artifacts for CI and local iteration.

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
- **Redirector**: Lightweight asset mapping from an old UUID/VirtualPath to a new one.
- **Cook Key**: Hash identifying the deterministic input set for an asset cook.

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
- UUIDs are stable across renames/moves; VirtualPath can change without re-import unless data depends on it.

### 3.0 VirtualPath Rules
- Always use forward slashes and ASCII: `Domain/Category/Name`.
- Case-insensitive compare at runtime; normalize to lower-case on disk.
- Reserved top-level prefixes: `Engine/`, `Demo/<Name>/`, `Tools/`.
- No file extensions in VirtualPath.

### 3.0.1 Renames, Moves, and Redirectors
- Renames/moves update VirtualPath in `.meta` but keep UUID unchanged.
- If UUID must change (e.g., duplicate UUID detected), emit a Redirector asset:
  - `OldUuid -> NewUuid`, `OldVirtualPath -> NewVirtualPath`.
  - Redirectors ship in registry for at least one release cycle.
- Runtime lookup:
  1) VirtualPath -> UUID
  2) Resolve Redirector (if present)
  3) Load final target UUID

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

## 3.3 Metadata File Conventions
- `.meta` format: JSON for Phase 0-2; consider TOML/YAML later if tooling benefits.
- Store authoring hints under `ImportSettings` (non-runtime).
- Avoid absolute paths. Use repo-root-relative `SourcePath`.
- Add `CookKey` and `LastCooked` stamps to support incremental builds.

Example extensions (sketch):
```json
{
  "ImportSettings": { "SRGB": true, "MipGen": "Default" },
  "CookKey": "sha256:0c7f...e9",
  "LastCooked": "2026-02-10T12:00:00Z"
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

### 4.5 Loading Phases (Async-Ready)
Split loading into phases to support async and GPU finalization:
1) IO: read cooked bytes (background thread).
2) Decode: CPU-only parse/transform (background thread).
3) Finalize: GPU resource creation (main thread).

Expose optional hooks:
- `Preload()` for IO scheduling.
- `Finalize()` for render-thread integration.

### 4.6 Error Handling & Fallback
- Missing asset: return null + error log with VirtualPath + UUID.
- Loader mismatch: return null + typed error code.
- Optional fallback asset per type (e.g., checkerboard texture).

---

## 4.7 Asset Type Minimum Fields (v1)
Define a minimal `FAssetDesc` surface for each asset type:
- Texture2D: `Width`, `Height`, `Format`, `MipCount`, `SRGB`.
- Mesh: `VertexFormat`, `IndexFormat`, `Bounds`, `SubMeshes`.
- Material: `ShadingModel`, `TextureBindings` (UUIDs).
- Audio: `Codec`, `Channels`, `SampleRate`, `Duration`.

Keep the runtime `FAssetDesc` small; large authoring data stays in tooling.

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

### 5.3 Deterministic Builds & Cook Keys
- Compute `CookKey = hash(source bytes + import settings + importer version + platform config)`.
- Skip cook if `CookKey` unchanged and cooked output exists.
- Deterministic ordering for dependencies and registry output.

### 5.4 Output Layout (Dev)
```
<BuildRoot>/Cooked/<Platform>/
  Registry/AssetRegistry.json
  Assets/<Uuid>.bin
  Bundles/<BundleName>.pak
<BuildRoot>/Imported/
  <Uuid>.<importer>.bin
<BuildRoot>/Cache/
  CookKeys.json
```

### 5.5 CLI Surface (Draft)
Provide a thin CLI for automated workflows:
- `AssetTool import --root <RepoRoot> --platform Win64`
- `AssetTool cook --platform Win64 --demo Minimal`
- `AssetTool validate --registry <path>`
- `AssetTool clean --cache`

### 5.6 Configuration Files
`Assets/AssetPipeline.json` (or `config/AssetPipeline.json`):
```json
{
  "Platforms": ["Win64", "Linux"],
  "DefaultTextureFormat": "BC7",
  "AudioCodec": "OggVorbis",
  "CookedOutputRoot": "build/Cooked"
}
```

--- 

## 6. IO & Packaging
Runtime reads only cooked assets using `IFileSystem` abstraction:
- Local loose files (dev mode).
- Pak/Bundle files (shipping).

Bundling strategy (future):
- Chunk by virtual path prefix (`Engine/*`, `Demo/Minimal/*`).
- Support per-platform bundles.

### 6.1 Bundle Index
Store an index table mapping UUID -> offset/size in bundle.
Optional per-bundle compression/encryption metadata.

--- 

## 7. Versioning & Compatibility
Each asset format includes:
- Asset type version (per loader).
- Importer version (per importer).
- Optional schema version for registry.

When versions change:
- Require re-import/re-cook.
- Keep compatibility paths if necessary.

### 7.1 Registry Schema (v1)
Draft JSON schema (runtime-facing):
```json
{
  "SchemaVersion": 1,
  "Assets": [
    {
      "Uuid": "a9c1b3c0-6d6e-4e16-9f4d-2d0a4f2d9a0b",
      "Type": "Texture2D",
      "VirtualPath": "Engine/Textures/Wood_Albedo",
      "CookedPath": "Assets/a9c1b3c0.bin",
      "Dependencies": [],
      "Desc": { "Width": 1024, "Height": 1024, "Format": "BC7", "MipCount": 10, "SRGB": true }
    }
  ],
  "Redirectors": [
    {
      "OldUuid": "11111111-1111-1111-1111-111111111111",
      "NewUuid": "a9c1b3c0-6d6e-4e16-9f4d-2d0a4f2d9a0b",
      "OldVirtualPath": "Engine/Textures/Wood_Albedo_Old"
    }
  ]
}
```

--- 

## 8. Diagnostics
Add logging and validation:
- Missing asset warnings show VirtualPath + UUID.
- Dependency cycles detected during registry build.
- Cook validation emits a summary report.
- Registry validation checks UUID uniqueness and VirtualPath collisions.
- Bundle validation checks offsets and sizes.

---

## 9. Dependency Rules
- Hard dependency: required for load (e.g., Mesh -> Material).
- Soft dependency: optional/late load (e.g., LODs, thumbnails).
- Runtime dependency graph must be acyclic; tool enforces this.

---

## 10. Security & Integrity (Optional)
- Optional checksum per cooked asset (SHA-256).
- Bundle signing for shipping builds.
- Strip editor-only metadata from shipped registry.

--- 

## Implementation Plan (TODO)

### Phase 0: Structure & Stubs
- [x] Add `Source/Engine/Asset/` module (Public/Private).
- [x] Add `AE_ASSET_API` export macro in `AssetAPI.h`.
- [x] Wire CMake target `AltinaEngineAsset` and link to `AltinaEngineCore`.

### Phase 1: Runtime Core
- [x] Define `FAssetHandle`, `EAssetType`, `FAssetDesc`.
- [x] Implement `AssetRegistry` (JSON reader for bootstrap).
- [x] Implement `AssetManager` with simple cache + sync load.
- [x] Implement Redirector resolution.

### Phase 2: Tooling MVP
- [x] Add `Tools/AssetPipeline` skeleton (import + cook commands).
- [x] Implement `.meta` generation for textures and meshes.
- [x] Emit `AssetRegistry.json` for runtime load.
- [x] Add `CookKey` + incremental cook cache.
- [x] Add registry validation CLI.

### Phase 3: Asset Types
- [x] Texture2D loader (DDS/BCn or engine format).
- [x] Mesh loader (engine format).
- [ ] Material loader (references textures).
- [x] Audio loader (streaming-friendly format).

#### Mesh Loader (Engine Format) Draft
Goals:
- Fast runtime load with strict validation.
- Simple, stable binary layout with offsets and sizes.
- Directly consumable by Render/RHI (vertex layout + index type).

Binary layout (v1):
- `FAssetBlobHeader` + `FMeshBlobDesc`
- `FMeshVertexAttributeDesc[]`
- `FMeshSubMeshDesc[]`
- `VertexBuffer`
- `IndexBuffer`

All offsets in `FMeshBlobDesc` are **byte offsets from the blob start**
(immediately after `FMeshBlobDesc`), so the loader can bounds-check against
`header.DataSize`.

```cpp
struct FMeshBlobDesc {
  u32 VertexCount;
  u32 IndexCount;
  u32 VertexStride;
  u32 IndexType;        // ERhiIndexType (u16/u32)
  u32 AttributeCount;
  u32 SubMeshCount;

  u32 AttributesOffset; // bytes from blob start
  u32 SubMeshesOffset;  // bytes from blob start
  u32 VertexDataOffset; // bytes from blob start
  u32 IndexDataOffset;  // bytes from blob start

  u32 VertexDataSize;   // bytes
  u32 IndexDataSize;    // bytes

  f32 BoundsMin[3];     // optional in v1
  f32 BoundsMax[3];

  u32 Flags;            // e.g. Interleaved=1, HasBounds=2
};

struct FMeshVertexAttributeDesc {
  u32 Semantic;         // Position/Normal/Tangent/UV/Color (enum or hash)
  u32 SemanticIndex;    // UV0/UV1 etc.
  u32 Format;           // ERhiFormat
  u32 InputSlot;        // v1: 0 (single stream)
  u32 AlignedOffset;    // byte offset in vertex
  u32 PerInstance;      // 0/1
  u32 InstanceStepRate;
};

struct FMeshSubMeshDesc {
  u32 IndexStart;
  u32 IndexCount;
  i32 BaseVertex;
  u32 MaterialSlot;
};
```

Loader validation rules:
- `VertexCount > 0`, `IndexCount > 0`, `VertexStride > 0`.
- `VertexDataSize == VertexCount * VertexStride`.
- `IndexDataSize == IndexCount * (IndexType == u16 ? 2 : 4)`.
- `Offsets + Sizes` must be within `header.DataSize`.
- `AttributeCount` / `SubMeshCount` arrays fully readable.

Importer/cook (v1) rules:
- Single interleaved vertex stream.
- Triangles only.
- Index type u16 or u32.
- No LODs; no compression.

##### LOD Extension (Classic)
The current layout can be extended for classic LODs by adding a LOD table and per‑LOD
data ranges. This keeps validation simple and avoids conflating different LODs into
one giant submesh list.

Add to `FMeshBlobDesc`:
```cpp
u32 LodCount;
u32 LodTableOffset; // bytes from blob start
```

Per‑LOD table entry:
```cpp
struct FMeshLodDesc {
  u32 VertexCount;
  u32 IndexCount;
  u32 VertexDataOffset;
  u32 IndexDataOffset;
  u32 VertexDataSize;
  u32 IndexDataSize;

  u32 SubMeshOffset;
  u32 SubMeshCount;

  f32 ScreenSize;   // or PixelCoverage threshold
  f32 BoundsMin[3]; // optional
  f32 BoundsMax[3];
};
```

Loader validation additions:
- `LodTableOffset` + `LodCount * sizeof(FMeshLodDesc)` within `header.DataSize`.
- For each LOD: VB/IB ranges within bounds; size matches counts; submesh table in range.

Registry (runtime desc) additions (optional but recommended):
- `FMeshDesc` add `LodCount` and global `Bounds`.

##### Nanite / Cluster‑Based LOD
The classic blob layout is **not sufficient** for Nanite‑style data. It needs
cluster/meshlet tables, hierarchical culling metadata, and streaming page
indices. Recommended direction:
- Introduce a new asset type or blob version (e.g., `NaniteMesh`) rather than
  overloading the classic mesh loader.
- Add chunked/streamable IO support (loader reads only required ranges instead of
  loading the entire blob).
- Store per‑cluster bounds + error metrics + parent/child hierarchy in the cooked blob.

### Phase 4: Async Loading + Hot Reload
- [ ] Async load queue + IO thread.
- [ ] Main-thread finalize phase for GPU resources.
- [ ] Editor/dev hot reload based on `.meta` timestamp.

### Phase 5: Packaging
- [ ] Asset bundle/pak format with index table.
- [ ] Bundle per demo and per platform.
- [ ] Registry moved to binary format.
- [ ] Optional checksum/signing for bundles.

--- 

## Open Questions
- Preferred cooked texture format on Windows (DDS/BCn vs custom)?
- Do we need a shared global asset cache across demos?
- Should registry be JSON long-term or migrate to binary/SQLite?
- Should VirtualPath lookups be case-sensitive or normalized to lower-case only?
