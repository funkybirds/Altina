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
#include "Asset/ShaderLoader.h"
#include "Asset/ScriptLoader.h"
#include "Asset/Texture2DLoader.h"
#include "Input/InputMessageHandler.h"
#include "Input/InputSystem.h"
#include "Threading/RenderingThread.h"
#include "Scripting/ScriptSystemCoreCLR.h"
#include "Jobs/JobSystem.h"
#include "Rhi/RhiContext.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiViewport.h"
#include "Engine/Runtime/EngineRuntime.h"
#include "Engine/Runtime/MaterialCache.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "RenderAsset/MaterialShaderAssetLoader.h"
#include "RenderAsset/MeshAssetConversion.h"
#include "Asset/MeshAsset.h"
#include "Types/CheckedCast.h"

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
        [[nodiscard]] auto IsRunning() const noexcept -> bool { return mIsRunning; }
        [[nodiscard]] auto GetInputSystem() const noexcept -> const Input::FInputSystem*;
        [[nodiscard]] auto GetWorldManager() noexcept -> GameScene::FWorldManager&;
        [[nodiscard]] auto GetWorldManager() const noexcept -> const GameScene::FWorldManager&;
        [[nodiscard]] auto GetAssetRegistry() noexcept -> Asset::FAssetRegistry&;
        [[nodiscard]] auto GetAssetRegistry() const noexcept -> const Asset::FAssetRegistry&;
        [[nodiscard]] auto GetAssetManager() noexcept -> Asset::FAssetManager&;
        [[nodiscard]] auto GetAssetManager() const noexcept -> const Asset::FAssetManager&;
        auto               LoadDemoAssetRegistry() -> bool;

    private:
        static void BindMeshMaterialConverter(
            Asset::FAssetRegistry& registry, Asset::FAssetManager& manager) {
            GameScene::FMeshMaterialComponent::AssetToRenderMaterialConverter =
                [&registry, &manager](const Asset::FAssetHandle& handle,
                    const Asset::FMeshMaterialParameterBlock& parameters) -> RenderCore::FMaterial {
                return Rendering::BuildRenderMaterialFromAsset(
                    handle, parameters, registry, manager);
            };
        }

        static void BindStaticMeshConverter(
            Asset::FAssetRegistry& /*registry*/, Asset::FAssetManager& manager) {
            GameScene::FStaticMeshFilterComponent::AssetToStaticMeshConverter =
                [&manager](
                    const Asset::FAssetHandle& handle) -> RenderCore::Geometry::FStaticMeshData {
                if (!handle.IsValid()) {
                    return {};
                }

                const auto asset = manager.Load(handle);
                if (!asset) {
                    return {};
                }

                const auto* meshAsset =
                    AltinaEngine::CheckedCast<const Asset::FMeshAsset*>(asset.Get());
                if (meshAsset == nullptr) {
                    return {};
                }

                RenderCore::Geometry::FStaticMeshData mesh{};
                if (!Rendering::ConvertMeshAssetToStaticMesh(*meshAsset, mesh)) {
                    return {};
                }
                for (auto& lod : mesh.Lods) {
                    lod.PositionBuffer.InitResource();
                    lod.IndexBuffer.InitResource();
                    lod.TangentBuffer.InitResource();
                    lod.UV0Buffer.InitResource();
                    lod.UV1Buffer.InitResource();

                    lod.PositionBuffer.WaitForInit();
                    lod.IndexBuffer.WaitForInit();
                    lod.TangentBuffer.WaitForInit();
                    lod.UV0Buffer.WaitForInit();
                    lod.UV1Buffer.WaitForInit();
                }
                return mesh;
            };
        }

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
        Asset::FShaderLoader                 mShaderLoader;
        Asset::FScriptLoader                 mScriptLoader;
        Asset::FTexture2DLoader              mTexture2DLoader;
        Engine::FMaterialCache               mMaterialCache;
        TOwner<RenderCore::FRenderingThread> mRenderingThread;
        TQueue<Core::Jobs::FJobHandle>       mPendingRenderFrames;
        Engine::FEngineRuntime               mEngineRuntime{};
    };
} // namespace AltinaEngine::Launch
