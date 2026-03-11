#pragma once

#include "Base/LaunchAPI.h"
#include "CoreMinimal.h"
#include "Launch/RuntimeSession.h"
#include "Container/SmartPtr.h"
#include "Container/Function.h"
#include "Container/Queue.h"
#include "Application/Application.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Asset/AssetBinary.h"
#include "Asset/AudioLoader.h"
#include "Asset/LevelLoader.h"
#include "Asset/MaterialLoader.h"
#include "Asset/ModelLoader.h"
#include "Asset/MeshLoader.h"
#include "Asset/ShaderLoader.h"
#include "Asset/ScriptLoader.h"
#include "Asset/Texture2DLoader.h"
#include "Asset/CubeMapLoader.h"
#include "Input/InputMessageHandler.h"
#include "Input/InputSystem.h"
#include "Threading/RenderingThread.h"
#include "Jobs/JobSystem.h"
#include "Rhi/RhiContext.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiViewport.h"
#include "Engine/Runtime/EngineRuntime.h"
#include "Engine/Runtime/MaterialCache.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/SkyCubeComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "RenderAsset/MaterialShaderAssetLoader.h"
#include "RenderAsset/MeshAssetConversion.h"
#include "Asset/MeshAsset.h"
#include "Asset/CubeMapAsset.h"
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

namespace AltinaEngine::DebugGui {
    class IDebugGuiSystem;
} // namespace AltinaEngine::DebugGui

namespace AltinaEngine::Launch {
    namespace Container = Core::Container;

    class AE_LAUNCH_API FEngineLoop final : public IRuntimeSession {
    public:
        using FRenderCallback = TFunction<void(Rhi::FRhiDevice&, Rhi::FRhiViewport&, u32, u32)>;

        FEngineLoop() = default;
        explicit FEngineLoop(const FStartupParameters& InStartupParameters);
        ~FEngineLoop();

        auto               PreInit() -> bool override;
        auto               Init() -> bool override;
        auto               BeginFrame(const FFrameContext& frameContext) -> bool override;
        void               TickSimulation(const FSimulationTick& tick) override;
        void               RenderFrame(const FRenderTick& tick) override;
        void               EndFrame() override;
        void               Shutdown() override;
        [[nodiscard]] auto GetServices() noexcept -> FRuntimeServices override;
        [[nodiscard]] auto GetServices() const noexcept -> FRuntimeServicesConst override;
        [[nodiscard]] auto IsRunning() const noexcept -> bool override { return mIsRunning; }

        // Backward-compatible helper for existing gameplay clients.
        void               Tick(float InDeltaTime);
        void               Exit() { Shutdown(); }
        void               SetRenderCallback(FRenderCallback callback);
        void               SetRuntimeInputOverride(Input::FInputSystem* inputSystem) noexcept;
        [[nodiscard]] auto GetInputSystem() noexcept -> Input::FInputSystem*;
        [[nodiscard]] auto GetInputSystem() const noexcept -> const Input::FInputSystem*;
        [[nodiscard]] auto GetPlatformInputSystem() noexcept -> Input::FInputSystem*;
        [[nodiscard]] auto GetPlatformInputSystem() const noexcept -> const Input::FInputSystem*;
        [[nodiscard]] auto GetMainWindow() noexcept -> Application::FPlatformWindow*;
        [[nodiscard]] auto GetWorldManager() noexcept -> GameScene::FWorldManager&;
        [[nodiscard]] auto GetWorldManager() const noexcept -> const GameScene::FWorldManager&;
        [[nodiscard]] auto GetAssetRegistry() noexcept -> Asset::FAssetRegistry&;
        [[nodiscard]] auto GetAssetRegistry() const noexcept -> const Asset::FAssetRegistry&;
        [[nodiscard]] auto GetAssetManager() noexcept -> Asset::FAssetManager&;
        [[nodiscard]] auto GetAssetManager() const noexcept -> const Asset::FAssetManager&;
        [[nodiscard]] auto GetDebugGui() noexcept -> DebugGui::IDebugGuiSystem*;
        [[nodiscard]] auto GetDebugGui() const noexcept -> const DebugGui::IDebugGuiSystem*;
        auto               LoadDemoAssetRegistry() -> bool;

    private:
        struct FEditorOffscreenCache {
            Rhi::FRhiTextureRef Texture;
            u32                 Width  = 0U;
            u32                 Height = 0U;
            Rhi::ERhiFormat     Format = Rhi::ERhiFormat::Unknown;
        };

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
                for (auto& lod : mesh.mLods) {
                    lod.mPositionBuffer.InitResource();
                    lod.mIndexBuffer.InitResource();
                    lod.mTangentBuffer.InitResource();
                    lod.mUV0Buffer.InitResource();
                    lod.mUV1Buffer.InitResource();

                    lod.mPositionBuffer.WaitForInit();
                    lod.mIndexBuffer.WaitForInit();
                    lod.mTangentBuffer.WaitForInit();
                    lod.mUV0Buffer.WaitForInit();
                    lod.mUV1Buffer.WaitForInit();
                }
                return mesh;
            };
        }

        static void BindSkyCubeConverter(Asset::FAssetManager& manager) {
            GameScene::FSkyCubeComponent::AssetToSkyCubeConverter =
                [&manager](const Asset::FAssetHandle& handle)
                -> GameScene::FSkyCubeComponent::FSkyCubeRhiResources {
                GameScene::FSkyCubeComponent::FSkyCubeRhiResources out{};

                if (!handle.IsValid()) {
                    return out;
                }

                auto* device = Rhi::RHIGetDevice();
                if (device == nullptr) {
                    return out;
                }

                const auto assetRef = manager.Load(handle);
                if (!assetRef) {
                    return out;
                }

                const auto* cubeAsset =
                    AltinaEngine::CheckedCast<const Asset::FCubeMapAsset*>(assetRef.Get());
                if (cubeAsset == nullptr) {
                    return out;
                }

                const auto& assetDesc     = cubeAsset->GetDesc();
                const u32   bytesPerPixel = Asset::GetTextureBytesPerPixel(assetDesc.Format);
                if (bytesPerPixel == 0U || assetDesc.Size == 0U || assetDesc.MipCount == 0U) {
                    return out;
                }

                Rhi::ERhiFormat format = Rhi::ERhiFormat::Unknown;
                switch (assetDesc.Format) {
                    case Asset::kTextureFormatRGBA8:
                        format = assetDesc.SRGB ? Rhi::ERhiFormat::R8G8B8A8UnormSrgb
                                                : Rhi::ERhiFormat::R8G8B8A8Unorm;
                        break;
                    case Asset::kTextureFormatRGBA16F:
                        format = Rhi::ERhiFormat::R16G16B16A16Float;
                        break;
                    case Asset::kTextureFormatR8:
                    case Asset::kTextureFormatRGB8:
                    default:
                        // Cook pipeline should output supported formats; fall back to RGBA8.
                        format = assetDesc.SRGB ? Rhi::ERhiFormat::R8G8B8A8UnormSrgb
                                                : Rhi::ERhiFormat::R8G8B8A8Unorm;
                        break;
                }

                Rhi::FRhiTextureDesc texDesc{};
                texDesc.mDebugName.Assign(TEXT("SkyCube"));
                texDesc.mWidth       = assetDesc.Size;
                texDesc.mHeight      = assetDesc.Size;
                texDesc.mDepth       = 1U;
                texDesc.mMipLevels   = assetDesc.MipCount;
                texDesc.mArrayLayers = 6U;
                texDesc.mFormat      = format;
                texDesc.mDimension   = Rhi::ERhiTextureDimension::Cube;
                texDesc.mBindFlags   = Rhi::ERhiTextureBindFlags::ShaderResource;

                out.Texture = Rhi::RHICreateTexture(texDesc);
                if (!out.Texture) {
                    return {};
                }

                const auto& pixels = cubeAsset->GetPixels();
                if (!pixels.IsEmpty()) {
                    u32   size   = assetDesc.Size;
                    usize offset = 0U;
                    for (u32 mip = 0U; mip < assetDesc.MipCount; ++mip) {
                        const u32 rowPitch   = size * bytesPerPixel;
                        const u32 slicePitch = rowPitch * size;
                        if (rowPitch == 0U || slicePitch == 0U) {
                            break;
                        }
                        const usize faceSize = static_cast<usize>(slicePitch);
                        const usize mipSize  = faceSize * 6U;
                        if (offset + mipSize > pixels.Size()) {
                            break;
                        }

                        for (u32 face = 0U; face < 6U; ++face) {
                            Rhi::FRhiTextureSubresource subresource{};
                            subresource.mMipLevel   = mip;
                            subresource.mArrayLayer = face;
                            device->UpdateTextureSubresource(out.Texture.Get(), subresource,
                                pixels.Data() + offset, rowPitch, slicePitch);
                            offset += faceSize;
                        }

                        size = (size > 1U) ? (size >> 1U) : 1U;
                    }
                }

                Rhi::FRhiShaderResourceViewDesc srvDesc{};
                srvDesc.mDebugName.Assign(TEXT("SkyCube.SRV"));
                srvDesc.mTexture                      = out.Texture.Get();
                srvDesc.mFormat                       = texDesc.mFormat;
                srvDesc.mTextureRange.mBaseMip        = 0U;
                srvDesc.mTextureRange.mMipCount       = texDesc.mMipLevels;
                srvDesc.mTextureRange.mBaseArrayLayer = 0U;
                srvDesc.mTextureRange.mLayerCount     = texDesc.mArrayLayers;

                out.SRV = device->CreateShaderResourceView(srvDesc);
                return out;
            };
        }

        void                                Draw(const FRenderTick& tick);
        void                                FlushRenderFrames();
        void                                EnforceRenderLag(u32 maxLagFrames);

        TOwner<Input::FInputSystem>         mInputSystem;
        Input::FInputSystem*                mRuntimeInputOverride = nullptr;
        TOwner<Input::FInputMessageHandler> mAppMessageHandler;
        TOwner<Application::FApplication, TPolymorphicDeleter<Application::FApplication>>
            mApplication;
        TOwner<Scripting::CoreCLR::FScriptSystem,
            TPolymorphicDeleter<Scripting::CoreCLR::FScriptSystem>>
                                                                        mScriptSystem;
        TOwner<Rhi::FRhiContext, TPolymorphicDeleter<Rhi::FRhiContext>> mRhiContext;
        TShared<Rhi::FRhiDevice>                                        mRhiDevice;
        Rhi::FRhiViewportRef                                            mMainViewport;
        u32                                                             mViewportWidth  = 0U;
        u32                                                             mViewportHeight = 0U;
        u64                                                             mFrameIndex     = 0ULL;
        f32                                  mLastDeltaTimeSeconds                      = 0.0f;
        u64                                  mHostFrameIndex                            = 0ULL;
        bool                                 mFrameActive                               = false;
        FRenderCallback                      mRenderCallback;
        FStartupParameters                   mStartupParameters{};
        bool                                 mIsRunning  = false;
        bool                                 mAssetReady = false;
        Asset::FAssetRegistry                mAssetRegistry;
        Asset::FAssetManager                 mAssetManager;
        Asset::FAudioLoader                  mAudioLoader;
        Asset::FLevelLoader                  mLevelLoader;
        Asset::FMaterialLoader               mMaterialLoader;
        Asset::FModelLoader                  mModelLoader;
        Asset::FMeshLoader                   mMeshLoader;
        Asset::FShaderLoader                 mShaderLoader;
        Asset::FScriptLoader                 mScriptLoader;
        Asset::FTexture2DLoader              mTexture2DLoader;
        Asset::FCubeMapLoader                mCubeMapLoader;
        Engine::FMaterialCache               mMaterialCache;
        TOwner<RenderCore::FRenderingThread> mRenderingThread;
        TOwner<DebugGui::IDebugGuiSystem, TPolymorphicDeleter<DebugGui::IDebugGuiSystem>> mDebugGui;
        FEditorOffscreenCache          mEditorOffscreenCache{};
        TQueue<Core::Jobs::FJobHandle> mPendingRenderFrames;
        Engine::FEngineRuntime         mEngineRuntime{};
    };
} // namespace AltinaEngine::Launch
