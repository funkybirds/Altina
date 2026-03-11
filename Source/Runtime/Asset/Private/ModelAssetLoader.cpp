#include "Asset/ModelAssetLoader.h"
#include "Types/Traits.h"

using AltinaEngine::Move;

namespace AltinaEngine::Asset {
    auto FModelAssetLoader::Load(const FAssetHandle& handle) const -> FModelAssetLoadResult {
        FModelAssetLoadResult result{};
        if (mManager == nullptr || !handle.IsValid() || handle.mType != EAssetType::Model) {
            return result;
        }

        auto asset = mManager->Load(handle);
        if (!asset) {
            return result;
        }

        auto* model = static_cast<FModelAsset*>(asset.Get());
        if (model == nullptr) {
            return result;
        }

        result.mAsset = Move(asset);
        result.mModel = model;
        return result;
    }
} // namespace AltinaEngine::Asset
