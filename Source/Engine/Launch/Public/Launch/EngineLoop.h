#pragma once

#include "Base/LaunchAPI.h"
#include "CoreMinimal.h"
#include "Container/SmartPtr.h"
#include "Container/Function.h"
#include "Container/Queue.h"
#include "Application/Application.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Asset/AudioLoader.h"
#include "Asset/MaterialLoader.h"
#include "Asset/MeshLoader.h"
#include "Asset/Texture2DLoader.h"
#include "Jobs/JobSystem.h"
#include "Rhi/RhiContext.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiViewport.h"

namespace AltinaEngine::RenderCore {
    class FRenderingThread;
} // namespace AltinaEngine::RenderCore

namespace AltinaEngine::Input {
    class FInputMessageHandler;
    class FInputSystem;
} // namespace AltinaEngine::Input

namespace AltinaEngine::Launch {
    namespace Container = Core::Container;

    class AE_LAUNCH_API FEngineLoop final {
    public:
        using FRenderCallback =
            Container::TFunction<void(Rhi::FRhiDevice&, Rhi::FRhiViewport&, u32, u32)>;

        FEngineLoop() = default;
        explicit FEngineLoop(const FStartupParameters& InStartupParameters);
        ~FEngineLoop();

        auto               PreInit() -> bool;
        auto               Init() -> bool;
        void               Tick(float InDeltaTime);
        void               Exit();
        void               SetRenderCallback(FRenderCallback callback);
        [[nodiscard]] auto GetInputSystem() const noexcept -> const Input::FInputSystem*;

    private:
        void               FlushRenderFrames();
        void               EnforceRenderLag(u32 maxLagFrames);

        Container::TOwner<Input::FInputSystem>         mInputSystem;
        Container::TOwner<Input::FInputMessageHandler> mAppMessageHandler;
        Container::TOwner<Application::FApplication,
            Container::TPolymorphicDeleter<Application::FApplication>>
            mApplication;
        Container::TOwner<Rhi::FRhiContext, Container::TPolymorphicDeleter<Rhi::FRhiContext>>
                                            mRhiContext;
        Container::TShared<Rhi::FRhiDevice> mRhiDevice;
        Rhi::FRhiViewportRef                mMainViewport;
        u32                                 mViewportWidth  = 0U;
        u32                                 mViewportHeight = 0U;
        u64                                 mFrameIndex     = 0ULL;
        FRenderCallback                     mRenderCallback;
        FStartupParameters                  mStartupParameters{};
        bool                                mIsRunning  = false;
        bool                                mAssetReady = false;
        Asset::FAssetRegistry               mAssetRegistry;
        Asset::FAssetManager                mAssetManager;
        Asset::FAudioLoader                 mAudioLoader;
        Asset::FMaterialLoader              mMaterialLoader;
        Asset::FMeshLoader                  mMeshLoader;
        Asset::FTexture2DLoader             mTexture2DLoader;
        Container::TOwner<RenderCore::FRenderingThread> mRenderingThread;
        Container::TQueue<Core::Jobs::FJobHandle>        mPendingRenderFrames;
    };
} // namespace AltinaEngine::Launch
