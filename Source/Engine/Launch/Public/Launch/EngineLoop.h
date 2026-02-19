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
#include "Asset/ScriptLoader.h"
#include "Asset/Texture2DLoader.h"
#include "Jobs/JobSystem.h"
#include "Rhi/RhiContext.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiViewport.h"
#include "Engine/Runtime/EngineRuntime.h"
#include "Engine/Runtime/MaterialCache.h"

using AltinaEngine::Core::Container::TFunction;
using AltinaEngine::Core::Container::TOwner;
using AltinaEngine::Core::Container::TPolymorphicDeleter;
using AltinaEngine::Core::Container::TQueue;
using AltinaEngine::Core::Container::TShared;
namespace AltinaEngine::RenderCore {
    class FRenderingThread;
} // namespace AltinaEngine::RenderCore

namespace AltinaEngine::Input {
    class FInputMessageHandler;
    class FInputSystem;
} // namespace AltinaEngine::Input

namespace AltinaEngine::Scripting::CoreCLR {
    class FScriptSystem;
} // namespace AltinaEngine::Scripting::CoreCLR

namespace AltinaEngine::Launch {
    namespace Container = Core::Container;

    class AE_LAUNCH_API FEngineLoop final {
    public:
        using FRenderCallback = TFunction<void(Rhi::FRhiDevice&, Rhi::FRhiViewport&, u32, u32)>;

        FEngineLoop() = default;
        explicit FEngineLoop(const FStartupParameters& InStartupParameters);
        ~FEngineLoop();

        auto               PreInit() -> bool;
        auto               Init() -> bool;
        void               Tick(float InDeltaTime);
        void               Exit();
        void               SetRenderCallback(FRenderCallback callback);
        [[nodiscard]] auto GetInputSystem() const noexcept -> const Input::FInputSystem*;
        [[nodiscard]] auto GetWorldManager() noexcept -> GameScene::FWorldManager&;
        [[nodiscard]] auto GetWorldManager() const noexcept -> const GameScene::FWorldManager&;
        [[nodiscard]] auto GetAssetRegistry() noexcept -> Asset::FAssetRegistry&;
        [[nodiscard]] auto GetAssetRegistry() const noexcept -> const Asset::FAssetRegistry&;

    private:
        void                                Draw();
        void                                FlushRenderFrames();
        void                                EnforceRenderLag(u32 maxLagFrames);

        TOwner<Input::FInputSystem>         mInputSystem;
        TOwner<Input::FInputMessageHandler> mAppMessageHandler;
        TOwner<Application::FApplication, TPolymorphicDeleter<Application::FApplication>>
                                                                        mApplication;
        TOwner<Scripting::CoreCLR::FScriptSystem>                       mScriptSystem;
        TOwner<Rhi::FRhiContext, TPolymorphicDeleter<Rhi::FRhiContext>> mRhiContext;
        TShared<Rhi::FRhiDevice>                                        mRhiDevice;
        Rhi::FRhiViewportRef                                            mMainViewport;
        u32                                                             mViewportWidth  = 0U;
        u32                                                             mViewportHeight = 0U;
        u64                                                             mFrameIndex     = 0ULL;
        f32                                  mLastDeltaTimeSeconds                      = 0.0f;
        FRenderCallback                      mRenderCallback;
        FStartupParameters                   mStartupParameters{};
        bool                                 mIsRunning  = false;
        bool                                 mAssetReady = false;
        Asset::FAssetRegistry                mAssetRegistry;
        Asset::FAssetManager                 mAssetManager;
        Asset::FAudioLoader                  mAudioLoader;
        Asset::FMaterialLoader               mMaterialLoader;
        Asset::FMeshLoader                   mMeshLoader;
        Asset::FScriptLoader                 mScriptLoader;
        Asset::FTexture2DLoader              mTexture2DLoader;
        Engine::FMaterialCache               mMaterialCache;
        TOwner<RenderCore::FRenderingThread> mRenderingThread;
        TQueue<Core::Jobs::FJobHandle>       mPendingRenderFrames;
        Engine::FEngineRuntime               mEngineRuntime{};
    };
} // namespace AltinaEngine::Launch
