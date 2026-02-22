#pragma once

#include "Asset/AssetBinary.h"

#include <vector>

namespace AltinaEngine::Tools::AssetPipeline {
    struct FVec2 {
        float X = 0.0f;
        float Y = 0.0f;
    };

    struct FVec3 {
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;
    };

    struct FMeshBuildResult {
        std::vector<u8>                              VertexData;
        std::vector<u8>                              IndexData;
        std::vector<Asset::FMeshVertexAttributeDesc> Attributes;
        std::vector<Asset::FMeshSubMeshDesc>         SubMeshes;
        u32                                          VertexCount      = 0;
        u32                                          IndexCount       = 0;
        u32                                          VertexStride     = 0;
        u32                                          IndexType        = Asset::kMeshIndexTypeUint32;
        u32                                          VertexFormatMask = 0;
        float                                        BoundsMin[3]     = { 0.0f, 0.0f, 0.0f };
        float                                        BoundsMax[3]     = { 0.0f, 0.0f, 0.0f };
    };
} // namespace AltinaEngine::Tools::AssetPipeline
