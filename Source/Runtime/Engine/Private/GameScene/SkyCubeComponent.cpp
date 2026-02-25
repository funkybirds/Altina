#include "Engine/GameScene/SkyCubeComponent.h"

using AltinaEngine::Move;
namespace AltinaEngine::GameScene {
    void FSkyCubeComponent::SetCubeMapAsset(Asset::FAssetHandle handle) noexcept {
        mCubeMapAsset = Move(handle);
    }
} // namespace AltinaEngine::GameScene
