#pragma once

#include "Asset/AssetTypes.h"
#include "Utility/Uuid.h"

#include <filesystem>
#include <string>

namespace AltinaEngine::Tools::AssetPipeline {
    struct FAssetRecord {
        std::filesystem::path SourcePath;
        std::filesystem::path MetaPath;
        std::string           SourcePathRel;
        std::string           VirtualPath;
        Asset::EAssetType     Type = Asset::EAssetType::Unknown;
        std::string           ImporterName;
        u32                   ImporterVersion = 1U;
        FUuid                 Uuid;
    };
} // namespace AltinaEngine::Tools::AssetPipeline
