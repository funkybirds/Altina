# ModelAsset Layout and Loader Interface

This document defines the proposed ModelAsset binary layout and loader interface. The goal is to keep MeshAsset as pure geometry while ModelAsset describes composition (nodes, mesh references, and material bindings).

## Design Goals

- MeshAsset stays "geometry only" with submeshes and material slots.
- ModelAsset stores hierarchy + transforms + mesh references + material slots.
- ModelAsset owns material bindings, not MeshAsset.
- ModelAsset can instance the same MeshAsset across multiple nodes.

## Asset Type

Introduce a new asset type:

- `EAssetType::Model` (next available enum value).

## Binary Layout

ModelAsset uses the standard `FAssetBlobHeader` with `Type = Model`. The payload is a `FModelBlobDesc` followed by tightly packed arrays.

```
| FAssetBlobHeader |
| FModelBlobDesc   |
| FModelNodeDesc[] |
| FModelMeshRef[]  |
| FAssetHandle[]   |  // material slots
```

All offsets in `FModelBlobDesc` are relative to the start of the data block (immediately after `FModelBlobDesc`), matching the Mesh blob convention.

### Blob Descriptors

```cpp
struct FModelBlobDesc {
    u32 NodeCount = 0;
    u32 MeshRefCount = 0;
    u32 MaterialSlotCount = 0;
    u32 NodesOffset = 0;
    u32 MeshRefsOffset = 0;
    u32 MaterialSlotsOffset = 0;
};
```

### Nodes

Nodes define hierarchy and local transform. The transform matches `FSpatialTransform` (rotation quaternion, translation, scale), stored as floats for a stable binary layout.

```cpp
struct FModelNodeDesc {
    i32 ParentIndex = -1;   // -1 = root
    i32 MeshRefIndex = -1;  // -1 = no mesh
    f32 Translation[3] = { 0.0f, 0.0f, 0.0f };
    f32 Rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // XYZW
    f32 Scale[3] = { 1.0f, 1.0f, 1.0f };
};
```

### Mesh References

Each mesh reference points at a MeshAsset and declares how its submeshes map to model material slots.

```cpp
struct FModelMeshRef {
    FAssetHandle Mesh;
    u32          MaterialSlotOffset = 0;
    u32          MaterialSlotCount  = 0;
};
```

`MaterialSlotOffset/Count` slice into the `FAssetHandle[]` material slot table. The material slot table is ordered such that `MaterialSlotOffset + subMeshIndex` yields the material for that submesh.

### Material Slots

Material slots are `FAssetHandle` entries that reference `MaterialTemplate` or `MaterialInstance` assets. The renderer picks a material by submesh index.

## Loader Interface

Add a new loader in the Asset module.

```cpp
class AE_ASSET_API FModelLoader final : public IAssetLoader {
public:
    [[nodiscard]] auto CanLoad(EAssetType type) const noexcept -> bool override;
    auto Load(const FAssetDesc& desc, IAssetStream& stream) -> TShared<IAsset> override;
};
```

`Load` should:

1. Read `FAssetBlobHeader` and validate `Type == Model`.
2. Read `FModelBlobDesc`.
3. Read the node array, mesh ref array, and material slot array using offsets.
4. Build a runtime `FModelAsset` that provides:
   - `Nodes[]`
   - `MeshRefs[]`
   - `MaterialSlots[]`
   - Optional cached bounds or derived data.

## Runtime Asset Shape (Suggested)

```cpp
class AE_ASSET_API FModelAsset final : public IAsset {
public:
    TVector<FModelNodeDesc> Nodes;
    TVector<FModelMeshRef>  MeshRefs;
    TVector<FAssetHandle>   MaterialSlots;
};
```

## Usage Notes

- Submesh material selection:
  - For a mesh reference `ref`, submesh `i` uses `MaterialSlots[ref.MaterialSlotOffset + i]`.
- For nodes without mesh, `MeshRefIndex = -1`.
- Multiple nodes can reference the same `FModelMeshRef` to instance meshes.

## Import/Cook Responsibilities

- Importers (glTF/FBX) should:
  - Generate MeshAssets for each unique mesh.
  - Build ModelAsset nodes from the scene graph.
  - Fill `MaterialSlots` in the same order as submeshes/primitives.
- The ModelAsset registry entry should list dependencies on:
  - All referenced Mesh assets.
  - All referenced Materials (and their textures transitively).
