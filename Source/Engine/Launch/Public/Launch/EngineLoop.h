#pragma once

#include "Base/LaunchAPI.h"
#include "CoreMinimal.h"
#include "Container/SmartPtr.h"
#include "Container/Function.h"
#include "Application/Application.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Asset/AudioLoader.h"
#include "Asset/MeshLoader.h"
#include "Asset/Texture2DLoader.h"
#include "Rhi/RhiContext.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiViewport.h"

namespace AltinaEngine::Launch {
    class AE_LAUNCH_API FEngineLoop final {
    public:
        using FRenderCallback =
            Core::Container::TFunction<void(Rhi::FRhiDevice&, Rhi::FRhiViewport&, u32, u32)>;

        FEngineLoop() = default;
        explicit FEngineLoop(const FStartupParameters& InStartupParameters);

        auto PreInit() -> bool;
        auto Init() -> bool;
        void Tick(float InDeltaTime);
        void Exit();
        void SetRenderCallback(FRenderCallback callback);

    private:
        Core::Container::TOwner<Application::FApplication,
            Core::Container::TPolymorphicDeleter<Application::FApplication>>
            mApplication;
        Core::Container::TOwner<Rhi::FRhiContext,
            Core::Container::TPolymorphicDeleter<Rhi::FRhiContext>>
            mRhiContext;
        Core::Container::TShared<Rhi::FRhiDevice>          mRhiDevice;
        Rhi::FRhiViewportRef                               mMainViewport;
        u32                                               mViewportWidth = 0U;
        u32                                               mViewportHeight = 0U;
        u64                                               mFrameIndex = 0ULL;
        FRenderCallback                                   mRenderCallback;
        FStartupParameters mStartupParameters{};
        bool               mIsRunning = false;
        bool               mAssetReady = false;
        Asset::FAssetRegistry mAssetRegistry;
        Asset::FAssetManager  mAssetManager;
        Asset::FAudioLoader    mAudioLoader;
        Asset::FMeshLoader     mMeshLoader;
        Asset::FTexture2DLoader mTexture2DLoader;
    };
} // namespace AltinaEngine::Launch
