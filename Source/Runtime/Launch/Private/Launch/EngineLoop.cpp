#include "Launch/EngineLoop.h"

#include "Input/InputMessageHandler.h"
#include "Input/InputSystem.h"
#include "Engine/EngineReflection.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/ScriptComponent.h"
#include "Engine/Runtime/SceneBatching.h"
#include "Engine/Runtime/SceneView.h"
#include "Rendering/BasicDeferredRenderer.h"
#include "Rendering/BasicForwardRenderer.h"
#include "Rendering/CommonRendererResource.h"
#include "Rendering/PostProcess/PostProcessSettings.h"
#include "Rendering/TemporalAA/TemporalAA.h"
#include "Rendering/RenderingSettings.h"
#include "DebugGui/DebugGui.h"
#include "Asset/Texture2DAsset.h"

#if defined(AE_ENABLE_SCRIPTING_CORECLR) && AE_ENABLE_SCRIPTING_CORECLR
    #include "Scripting/ScriptSystemCoreCLR.h"
#endif

#include "Console/ConsoleVariable.h"
#include "Logging/Log.h"
#include "Platform/PlatformFileSystem.h"
#include "Utility/EngineConfig/EngineConfig.h"
#include "Utility/Filesystem/Path.h"
#include "Utility/Filesystem/PathUtils.h"
#include "Threading/RenderingThread.h"
#include "FrameGraph/FrameGraph.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiCommandContext.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiStructs.h"
#include "Rhi/Command/RhiCmdContextAdapter.h"
#include "Reflection/Reflection.h"

#include <cstdint>
#include <cstring>
#include <string>

#if AE_PLATFORM_WIN
    #if defined(AE_ENABLE_SCRIPTING_CORECLR) && AE_ENABLE_SCRIPTING_CORECLR
        #ifdef TEXT
            #undef TEXT
        #endif
        #ifndef WIN32_LEAN_AND_MEAN
            #define WIN32_LEAN_AND_MEAN
        #endif
        #ifndef NOMINMAX
            #define NOMINMAX
        #endif
        #include <windows.h>
        #ifdef TEXT
            #undef TEXT
        #endif
        #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
            #define TEXT(str) L##str
        #else
            #define TEXT(str) str
        #endif
    #endif
    #include "Application/Windows/WindowsApplication.h"
    #include "RhiD3D11/RhiD3D11Context.h"
#elif AE_PLATFORM_MACOS
    #if defined(AE_ENABLE_SCRIPTING_CORECLR) && AE_ENABLE_SCRIPTING_CORECLR
        #include <mach-o/dyld.h>
    #endif
#else
    #include "RhiMock/RhiMockContext.h"
#endif

using AltinaEngine::Move;
using AltinaEngine::Core::Container::MakeUnique;
using AltinaEngine::Core::Container::MakeUniqueAs;
using AltinaEngine::Core::Container::TVector;
using AltinaEngine::Core::Logging::LogWarningCat;

namespace AltinaEngine::Launch {
    namespace Container = Core::Container;
    namespace {
        void EnsureEngineReflectionRegistered() {
            static bool sRegistered = false;
            if (!sRegistered) {
                Engine::RegisterEngineReflection();
                sRegistered = true;
            }
        }

#if defined(AE_ENABLE_SCRIPTING_CORECLR) && AE_ENABLE_SCRIPTING_CORECLR
        GameScene::FWorldManager*     gScriptWorldManager        = nullptr;
        Application::FPlatformWindow* gMainWindowForManagedTitle = nullptr;

        auto ToFStringFromUtf8Bytes(const char* data) -> Core::Container::FString {
            using Core::Container::FString;
            if (data == nullptr || data[0] == '\0') {
                return {};
            }
            const usize length = std::strlen(data);
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        #if AE_PLATFORM_WIN
            FString out;
            int     wideCount =
                MultiByteToWideChar(CP_UTF8, 0, data, static_cast<int>(length), nullptr, 0);
            if (wideCount <= 0) {
                return out;
            }
            std::wstring wide(static_cast<size_t>(wideCount), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, data, static_cast<int>(length), wide.data(), wideCount);
            out.Append(wide.c_str(), wide.size());
            return out;
        #else
            FString out;
            for (usize i = 0; i < length; ++i) {
                out.Append(static_cast<TChar>(static_cast<unsigned char>(data[i])));
            }
            return out;
        #endif
    #else
            // Non-unicode builds: treat input as native bytes.
            return FString(data, length);
    #endif
        }

        void SetScriptWindowTitleUtf8(const char* titleUtf8) {
            if (gMainWindowForManagedTitle == nullptr) {
                return;
            }
            const auto title = ToFStringFromUtf8Bytes(titleUtf8);
            if (title.IsEmptyString()) {
                return;
            }
            gMainWindowForManagedTitle->SetTitle(title);
        }

        auto GetScriptWorldTranslation(u32 worldId, u32 ownerIndex, u32 ownerGeneration,
            Scripting::FScriptVector3* outValue) -> bool {
            if (gScriptWorldManager == nullptr || outValue == nullptr) {
                return false;
            }

            GameScene::FWorldHandle handle{};
            handle.Id   = worldId;
            auto* world = gScriptWorldManager->GetWorld(handle);
            if (world == nullptr) {
                return false;
            }

            GameScene::FGameObjectId objectId{};
            objectId.Index      = ownerIndex;
            objectId.Generation = ownerGeneration;
            objectId.WorldId    = worldId;
            if (!world->IsAlive(objectId)) {
                return false;
            }

            const auto transform = world->Object(objectId).GetWorldTransform();
            outValue->X          = transform.Translation.X();
            outValue->Y          = transform.Translation.Y();
            outValue->Z          = transform.Translation.Z();
            return true;
        }

        auto SetScriptWorldTranslation(u32 worldId, u32 ownerIndex, u32 ownerGeneration,
            const Scripting::FScriptVector3* value) -> bool {
            if (gScriptWorldManager == nullptr || value == nullptr) {
                return false;
            }

            GameScene::FWorldHandle handle{};
            handle.Id   = worldId;
            auto* world = gScriptWorldManager->GetWorld(handle);
            if (world == nullptr) {
                return false;
            }

            GameScene::FGameObjectId objectId{};
            objectId.Index      = ownerIndex;
            objectId.Generation = ownerGeneration;
            objectId.WorldId    = worldId;
            if (!world->IsAlive(objectId)) {
                return false;
            }

            auto view             = world->Object(objectId);
            auto transform        = view.GetWorldTransform();
            transform.Translation = Core::Math::FVector3f(value->X, value->Y, value->Z);
            view.SetWorldTransform(transform);
            return true;
        }

        auto GetScriptLocalTranslation(u32 worldId, u32 ownerIndex, u32 ownerGeneration,
            Scripting::FScriptVector3* outValue) -> bool {
            if (gScriptWorldManager == nullptr || outValue == nullptr) {
                return false;
            }

            GameScene::FWorldHandle handle{};
            handle.Id   = worldId;
            auto* world = gScriptWorldManager->GetWorld(handle);
            if (world == nullptr) {
                return false;
            }

            GameScene::FGameObjectId objectId{};
            objectId.Index      = ownerIndex;
            objectId.Generation = ownerGeneration;
            objectId.WorldId    = worldId;
            if (!world->IsAlive(objectId)) {
                return false;
            }

            const auto transform = world->Object(objectId).GetLocalTransform();
            outValue->X          = transform.Translation.X();
            outValue->Y          = transform.Translation.Y();
            outValue->Z          = transform.Translation.Z();
            return true;
        }

        auto SetScriptLocalTranslation(u32 worldId, u32 ownerIndex, u32 ownerGeneration,
            const Scripting::FScriptVector3* value) -> bool {
            if (gScriptWorldManager == nullptr || value == nullptr) {
                return false;
            }

            GameScene::FWorldHandle handle{};
            handle.Id   = worldId;
            auto* world = gScriptWorldManager->GetWorld(handle);
            if (world == nullptr) {
                return false;
            }

            GameScene::FGameObjectId objectId{};
            objectId.Index      = ownerIndex;
            objectId.Generation = ownerGeneration;
            objectId.WorldId    = worldId;
            if (!world->IsAlive(objectId)) {
                return false;
            }

            auto view             = world->Object(objectId);
            auto transform        = view.GetLocalTransform();
            transform.Translation = Core::Math::FVector3f(value->X, value->Y, value->Z);
            view.SetLocalTransform(transform);
            return true;
        }

        auto GetScriptWorldRotation(u32 worldId, u32 ownerIndex, u32 ownerGeneration,
            Scripting::FScriptQuaternion* outValue) -> bool {
            if (gScriptWorldManager == nullptr || outValue == nullptr) {
                return false;
            }

            GameScene::FWorldHandle handle{};
            handle.Id   = worldId;
            auto* world = gScriptWorldManager->GetWorld(handle);
            if (world == nullptr) {
                return false;
            }

            GameScene::FGameObjectId objectId{};
            objectId.Index      = ownerIndex;
            objectId.Generation = ownerGeneration;
            objectId.WorldId    = worldId;
            if (!world->IsAlive(objectId)) {
                return false;
            }

            const auto transform = world->Object(objectId).GetWorldTransform();
            outValue->X          = transform.Rotation.x;
            outValue->Y          = transform.Rotation.y;
            outValue->Z          = transform.Rotation.z;
            outValue->W          = transform.Rotation.w;
            return true;
        }

        auto SetScriptWorldRotation(u32 worldId, u32 ownerIndex, u32 ownerGeneration,
            const Scripting::FScriptQuaternion* value) -> bool {
            if (gScriptWorldManager == nullptr || value == nullptr) {
                return false;
            }

            GameScene::FWorldHandle handle{};
            handle.Id   = worldId;
            auto* world = gScriptWorldManager->GetWorld(handle);
            if (world == nullptr) {
                return false;
            }

            GameScene::FGameObjectId objectId{};
            objectId.Index      = ownerIndex;
            objectId.Generation = ownerGeneration;
            objectId.WorldId    = worldId;
            if (!world->IsAlive(objectId)) {
                return false;
            }

            auto view      = world->Object(objectId);
            auto transform = view.GetWorldTransform();
            transform.Rotation =
                Core::Math::FQuaternion(value->X, value->Y, value->Z, value->W).Normalized();
            view.SetWorldTransform(transform);
            return true;
        }

        auto GetScriptLocalRotation(u32 worldId, u32 ownerIndex, u32 ownerGeneration,
            Scripting::FScriptQuaternion* outValue) -> bool {
            if (gScriptWorldManager == nullptr || outValue == nullptr) {
                return false;
            }

            GameScene::FWorldHandle handle{};
            handle.Id   = worldId;
            auto* world = gScriptWorldManager->GetWorld(handle);
            if (world == nullptr) {
                return false;
            }

            GameScene::FGameObjectId objectId{};
            objectId.Index      = ownerIndex;
            objectId.Generation = ownerGeneration;
            objectId.WorldId    = worldId;
            if (!world->IsAlive(objectId)) {
                return false;
            }

            const auto transform = world->Object(objectId).GetLocalTransform();
            outValue->X          = transform.Rotation.x;
            outValue->Y          = transform.Rotation.y;
            outValue->Z          = transform.Rotation.z;
            outValue->W          = transform.Rotation.w;
            return true;
        }

        auto SetScriptLocalRotation(u32 worldId, u32 ownerIndex, u32 ownerGeneration,
            const Scripting::FScriptQuaternion* value) -> bool {
            if (gScriptWorldManager == nullptr || value == nullptr) {
                return false;
            }

            GameScene::FWorldHandle handle{};
            handle.Id   = worldId;
            auto* world = gScriptWorldManager->GetWorld(handle);
            if (world == nullptr) {
                return false;
            }

            GameScene::FGameObjectId objectId{};
            objectId.Index      = ownerIndex;
            objectId.Generation = ownerGeneration;
            objectId.WorldId    = worldId;
            if (!world->IsAlive(objectId)) {
                return false;
            }

            auto view      = world->Object(objectId);
            auto transform = view.GetLocalTransform();
            transform.Rotation =
                Core::Math::FQuaternion(value->X, value->Y, value->Z, value->W).Normalized();
            view.SetLocalTransform(transform);
            return true;
        }
#endif

        auto GetRenderThreadLagFrames() noexcept -> u32 {
            int value = RenderCore::gRenderingThreadLagFrames.Get();
            if (value < 0)
                value = 0;
            return static_cast<u32>(value);
        }

        auto ResolveViewOutputTarget(const Engine::FSceneView& view,
            Rhi::FRhiViewport* fallbackViewport) noexcept -> Rhi::FRhiTexture* {
            using ETargetType = Engine::FSceneView::ETargetType;
            switch (view.Target.Type) {
                case ETargetType::Viewport:
                    if (view.Target.Viewport != nullptr) {
                        return view.Target.Viewport->GetBackBuffer();
                    }
                    break;
                case ETargetType::TextureAsset:
                    return nullptr;
                default:
                    break;
            }

            if (fallbackViewport != nullptr) {
                return fallbackViewport->GetBackBuffer();
            }
            return nullptr;
        }

        constexpr u64      kViewKeyHashSeed = 0x9e3779b97f4a7c15ULL;

        [[nodiscard]] auto HashCombine(u64 seed, u64 value) noexcept -> u64 {
            return seed ^ (value + kViewKeyHashSeed + (seed << 6U) + (seed >> 2U));
        }

        [[nodiscard]] auto HashPointer(const void* ptr) noexcept -> u64 {
            return static_cast<u64>(reinterpret_cast<uintptr_t>(ptr));
        }

        [[nodiscard]] auto HashUuid(const FUuid& uuid) noexcept -> u64 {
            // Simple FNV-1a over 16 bytes.
            constexpr u64 kOffset = 1469598103934665603ULL;
            constexpr u64 kPrime  = 1099511628211ULL;
            u64           h       = kOffset;
            const u8*     p       = uuid.Data();
            for (usize i = 0; i < FUuid::kByteCount; ++i) {
                h ^= static_cast<u64>(p[i]);
                h *= kPrime;
            }
            return h;
        }

        [[nodiscard]] auto ComputeTemporalViewKey(const Engine::FSceneView& view) noexcept -> u64 {
            using ETargetType = Engine::FSceneView::ETargetType;

            u64 h = 0ULL;
            h     = HashCombine(h, static_cast<u64>(view.Target.Type));

            // View target contributes to key.
            switch (view.Target.Type) {
                case ETargetType::Viewport:
                    h = HashCombine(h, HashPointer(view.Target.Viewport));
                    break;
                case ETargetType::TextureAsset:
                    h = HashCombine(h, HashUuid(view.Target.Texture.Uuid));
                    h = HashCombine(h, static_cast<u64>(view.Target.Texture.Type));
                    break;
                default:
                    break;
            }

            // Camera id contributes to key.
            h = HashCombine(h, static_cast<u64>(view.CameraId.Index));
            h = HashCombine(h, static_cast<u64>(view.CameraId.Generation));
            h = HashCombine(h, static_cast<u64>(view.CameraId.Type));
            return h;
        }

        void ExecuteFrameGraph(Rhi::FRhiDevice& device, RenderCore::FFrameGraph& graph) {
            Rhi::FRhiCommandContextDesc ctxDesc{};
            ctxDesc.mQueueType  = Rhi::ERhiQueueType::Graphics;
            auto commandContext = device.CreateCommandContext(ctxDesc);
            if (!commandContext) {
                return;
            }

            auto* ops = dynamic_cast<Rhi::IRhiCmdContextOps*>(commandContext.Get());
            if (ops == nullptr) {
                return;
            }

            Rhi::FRhiCmdContextAdapter adapter(*commandContext.Get(), *ops);
            adapter.Begin();
            graph.Execute(adapter);
            adapter.End();

            auto* commandList = commandContext->GetCommandList();
            if (commandList == nullptr) {
                return;
            }

            auto queue = device.GetQueue(Rhi::ERhiQueueType::Graphics);
            if (!queue) {
                return;
            }

            Rhi::FRhiCommandList* commandLists[] = { commandList };
            Rhi::FRhiSubmitInfo   submit{};
            submit.mCommandLists     = commandLists;
            submit.mCommandListCount = 1U;
            queue->Submit(submit);
        }

        void SendSceneRenderingRequest(Rhi::FRhiDevice& device, Rhi::FRhiViewport* defaultViewport,
            Engine::FRenderScene& scene, const TVector<RenderCore::Render::FDrawList>& drawLists,
            const TVector<RenderCore::Render::FDrawList>& shadowDrawLists,
            Rendering::ERendererType rendererType, const Asset::FAssetRegistry* assetRegistry,
            Asset::FAssetManager* assetManager) {
            if (scene.Views.IsEmpty()) {
                return;
            }

            // Cache skybox GPU resources on the render thread to avoid re-uploading every frame.
            static Asset::FAssetHandle                                sSkyCubeAsset{};
            static GameScene::FSkyCubeComponent::FSkyCubeRhiResources sSkyCubeRhi{};
            static bool                                               sHasSkyCubeRhi = false;

            // Cache IBL GPU resources (derived from the active sky cubemap).
            static Asset::FAssetHandle                                sSkyIrradianceAsset{};
            static GameScene::FSkyCubeComponent::FSkyCubeRhiResources sSkyIrradianceRhi{};
            static bool                                               sHasSkyIrradianceRhi = false;

            static Asset::FAssetHandle                                sSkySpecularAsset{};
            static GameScene::FSkyCubeComponent::FSkyCubeRhiResources sSkySpecularRhi{};
            static bool                                               sHasSkySpecularRhi = false;

            static Asset::FAssetHandle                                sBrdfLutAsset{};
            static Rhi::FRhiTextureRef                                sBrdfLutTexture{};
            static bool                                               sHasBrdfLutTexture = false;

            Rhi::FRhiTexture*                                         skyCubeTexture = nullptr;
            bool                                                      bHasSkyCube    = false;
            if (scene.bHasSkyCube && scene.SkyCubeAsset.IsValid()) {
                bHasSkyCube = true;
                if (!GameScene::FSkyCubeComponent::AssetToSkyCubeConverter) {
                    bHasSkyCube = false;
                } else if (!sHasSkyCubeRhi || sSkyCubeAsset != scene.SkyCubeAsset) {
                    sSkyCubeAsset = scene.SkyCubeAsset;
                    sSkyCubeRhi =
                        GameScene::FSkyCubeComponent::AssetToSkyCubeConverter(scene.SkyCubeAsset);
                    sHasSkyCubeRhi = sSkyCubeRhi.IsValid();

                    // Sky asset changed; derived IBL assets are tied to its virtual path.
                    sSkyIrradianceAsset  = {};
                    sHasSkyIrradianceRhi = false;
                    sSkySpecularAsset    = {};
                    sHasSkySpecularRhi   = false;
                    sBrdfLutAsset        = {};
                    sHasBrdfLutTexture   = false;
                    sBrdfLutTexture.Reset();
                }
                if (sHasSkyCubeRhi) {
                    skyCubeTexture = sSkyCubeRhi.Texture.Get();
                } else {
                    bHasSkyCube = false;
                }
            }

            // Optional environment IBL (diffuse irradiance + specular prefilter + BRDF LUT).
            Rhi::FRhiTexture* skyIrradiance  = nullptr;
            Rhi::FRhiTexture* skySpecular    = nullptr;
            Rhi::FRhiTexture* brdfLut        = nullptr;
            float             specularMaxLod = 0.0f;
            bool              bHasSkyIbl     = false;

            if (bHasSkyCube && assetRegistry != nullptr && assetManager != nullptr) {
                const auto* baseDesc = assetRegistry->GetDesc(scene.SkyCubeAsset);
                if (baseDesc != nullptr && !baseDesc->VirtualPath.IsEmpty()) {
                    auto MakeDerivedPath = [&](const TChar* suffix) -> Container::FString {
                        Container::FString out = baseDesc->VirtualPath;
                        out.Append(TEXT("/"));
                        out.Append(suffix);
                        return out;
                    };

                    const auto irradiancePath = MakeDerivedPath(TEXT("irradiance"));
                    const auto specularPath   = MakeDerivedPath(TEXT("specular"));
                    const auto brdfPath       = MakeDerivedPath(TEXT("brdf_lut"));

                    const auto irradianceHandle =
                        assetRegistry->FindByPath(irradiancePath.ToView());
                    const auto specularHandle = assetRegistry->FindByPath(specularPath.ToView());
                    const auto brdfHandle     = assetRegistry->FindByPath(brdfPath.ToView());

                    if (irradianceHandle.IsValid()
                        && GameScene::FSkyCubeComponent::AssetToSkyCubeConverter) {
                        if (!sHasSkyIrradianceRhi || sSkyIrradianceAsset != irradianceHandle) {
                            sSkyIrradianceAsset = irradianceHandle;
                            sSkyIrradianceRhi =
                                GameScene::FSkyCubeComponent::AssetToSkyCubeConverter(
                                    irradianceHandle);
                            sHasSkyIrradianceRhi = sSkyIrradianceRhi.IsValid();
                        }
                        if (sHasSkyIrradianceRhi) {
                            skyIrradiance = sSkyIrradianceRhi.Texture.Get();
                        }
                    }

                    if (specularHandle.IsValid()
                        && GameScene::FSkyCubeComponent::AssetToSkyCubeConverter) {
                        if (!sHasSkySpecularRhi || sSkySpecularAsset != specularHandle) {
                            sSkySpecularAsset = specularHandle;
                            sSkySpecularRhi = GameScene::FSkyCubeComponent::AssetToSkyCubeConverter(
                                specularHandle);
                            sHasSkySpecularRhi = sSkySpecularRhi.IsValid();
                        }
                        if (sHasSkySpecularRhi) {
                            skySpecular = sSkySpecularRhi.Texture.Get();
                        }

                        const auto* specDesc = assetRegistry->GetDesc(specularHandle);
                        if (specDesc != nullptr
                            && specDesc->Handle.Type == Asset::EAssetType::CubeMap
                            && specDesc->CubeMap.MipCount > 0U) {
                            specularMaxLod = static_cast<float>(specDesc->CubeMap.MipCount - 1U);
                        }
                    }

                    if (brdfHandle.IsValid()) {
                        const auto* brdfDesc = assetRegistry->GetDesc(brdfHandle);
                        if (brdfDesc == nullptr
                            || brdfDesc->Handle.Type != Asset::EAssetType::Texture2D) {
                            // Ignore invalid derived assets.
                        } else if (!sHasBrdfLutTexture || sBrdfLutAsset != brdfHandle) {
                            sBrdfLutAsset      = brdfHandle;
                            sHasBrdfLutTexture = false;
                            sBrdfLutTexture.Reset();

                            auto* rhiDevice = Rhi::RHIGetDevice();
                            if (rhiDevice != nullptr) {
                                const auto  assetRef = assetManager->Load(brdfHandle);
                                const auto* texAsset = assetRef
                                    ? static_cast<const Asset::FTexture2DAsset*>(assetRef.Get())
                                    : nullptr;
                                if (texAsset != nullptr) {
                                    const auto& assetDesc = texAsset->GetDesc();
                                    const u32   bytesPerPixel =
                                        Asset::GetTextureBytesPerPixel(assetDesc.Format);
                                    if (bytesPerPixel > 0U && assetDesc.Width > 0U
                                        && assetDesc.Height > 0U) {
                                        Rhi::ERhiFormat format = Rhi::ERhiFormat::Unknown;
                                        switch (assetDesc.Format) {
                                            case Asset::kTextureFormatRGBA8:
                                                format = assetDesc.SRGB
                                                    ? Rhi::ERhiFormat::R8G8B8A8UnormSrgb
                                                    : Rhi::ERhiFormat::R8G8B8A8Unorm;
                                                break;
                                            case Asset::kTextureFormatRGBA16F:
                                                format = Rhi::ERhiFormat::R16G16B16A16Float;
                                                break;
                                            case Asset::kTextureFormatR8:
                                            case Asset::kTextureFormatRGB8:
                                            default:
                                                format = assetDesc.SRGB
                                                    ? Rhi::ERhiFormat::R8G8B8A8UnormSrgb
                                                    : Rhi::ERhiFormat::R8G8B8A8Unorm;
                                                break;
                                        }

                                        Rhi::FRhiTextureDesc texDesc{};
                                        texDesc.mDebugName.Assign(TEXT("IBL.BrdfLut"));
                                        texDesc.mDimension = Rhi::ERhiTextureDimension::Tex2D;
                                        texDesc.mWidth     = assetDesc.Width;
                                        texDesc.mHeight    = assetDesc.Height;
                                        texDesc.mMipLevels =
                                            (assetDesc.MipCount > 0U) ? assetDesc.MipCount : 1U;
                                        texDesc.mFormat = format;
                                        texDesc.mBindFlags =
                                            Rhi::ERhiTextureBindFlags::ShaderResource;

                                        sBrdfLutTexture = Rhi::RHICreateTexture(texDesc);
                                        if (sBrdfLutTexture) {
                                            const auto& pixels = texAsset->GetPixels();
                                            if (!pixels.IsEmpty()) {
                                                u32   width  = assetDesc.Width;
                                                u32   height = assetDesc.Height;
                                                usize offset = 0U;
                                                for (u32 mip = 0U; mip < texDesc.mMipLevels;
                                                    ++mip) {
                                                    const usize rowPitch =
                                                        static_cast<usize>(width) * bytesPerPixel;
                                                    const usize slicePitch =
                                                        rowPitch * static_cast<usize>(height);
                                                    if (rowPitch == 0U || slicePitch == 0U) {
                                                        break;
                                                    }
                                                    if (offset + slicePitch > pixels.Size()) {
                                                        break;
                                                    }
                                                    Rhi::FRhiTextureSubresource subresource{};
                                                    subresource.mMipLevel = mip;
                                                    rhiDevice->UpdateTextureSubresource(
                                                        sBrdfLutTexture.Get(), subresource,
                                                        pixels.Data() + offset,
                                                        static_cast<u32>(rowPitch),
                                                        static_cast<u32>(slicePitch));
                                                    offset += slicePitch;
                                                    width  = (width > 1U) ? (width >> 1U) : 1U;
                                                    height = (height > 1U) ? (height >> 1U) : 1U;
                                                }
                                            }
                                            sHasBrdfLutTexture = true;
                                        }
                                    }
                                }
                            }
                        }
                        if (sHasBrdfLutTexture) {
                            brdfLut = sBrdfLutTexture.Get();
                        }
                    }

                    bHasSkyIbl = (skyIrradiance != nullptr) && (skySpecular != nullptr)
                        && (brdfLut != nullptr);
                }
            }

            Rendering::FBasicDeferredRenderer deferredRenderer;
            Rendering::FBasicForwardRenderer  forwardRenderer;
            Rendering::IRenderer* renderer = (rendererType == Rendering::ERendererType::Deferred)
                ? static_cast<Rendering::IRenderer*>(&deferredRenderer)
                : static_cast<Rendering::IRenderer*>(&forwardRenderer);

            renderer->PrepareForRendering(device);

            const usize viewCount = scene.Views.Size();
            for (usize i = 0; i < viewCount; ++i) {
                auto& view = scene.Views[i];
                if (!view.View.IsValid()) {
                    continue;
                }

                auto* outputTarget = ResolveViewOutputTarget(view, defaultViewport);
                if (outputTarget == nullptr) {
                    continue;
                }

                const u64  viewKey    = ComputeTemporalViewKey(view);
                const bool bEnableTaa = (rendererType == Rendering::ERendererType::Deferred)
                    && (Rendering::rPostProcessTaa.Get() != 0);
                const bool bEnableJitter  = bEnableTaa && (Rendering::rTemporalJitter.Get() != 0);
                i32        sampleCountI32 = Rendering::rTemporalJitterSampleCount.Get();
                if (sampleCountI32 < 1) {
                    sampleCountI32 = 1;
                }
                const u32 sampleCount = static_cast<u32>(sampleCountI32);

                // Prepare persistent previous-view snapshot + per-frame jitter (render thread).
                Rendering::TemporalAA::PrepareViewForFrame(
                    viewKey, view.View, bEnableJitter, sampleCount);

                Rendering::FRenderViewContext viewContext{};
                viewContext.ViewKey  = viewKey;
                viewContext.View     = &view.View;
                viewContext.DrawList = (i < drawLists.Size()) ? &drawLists[i] : nullptr;
                viewContext.ShadowDrawList =
                    (i < shadowDrawLists.Size()) ? &shadowDrawLists[i] : nullptr;
                viewContext.OutputTarget      = outputTarget;
                viewContext.Lights            = &scene.Lights;
                viewContext.SkyCubeTexture    = skyCubeTexture;
                viewContext.bHasSkyCube       = bHasSkyCube;
                viewContext.SkyIrradianceCube = skyIrradiance;
                viewContext.SkySpecularCube   = skySpecular;
                viewContext.BrdfLutTexture    = brdfLut;
                viewContext.SkySpecularMaxLod = specularMaxLod;
                viewContext.bHasSkyIbl        = bHasSkyIbl;
                renderer->SetViewContext(viewContext);

                RenderCore::FFrameGraph graph(device);
                renderer->Render(graph);
                graph.Compile();
                ExecuteFrameGraph(device, graph);

                Rendering::TemporalAA::FinalizeViewForFrame(
                    viewKey, view.View, bEnableJitter, sampleCount);
            }

            renderer->FinalizeRendering();
        }

#if defined(AE_ENABLE_SCRIPTING_CORECLR) && AE_ENABLE_SCRIPTING_CORECLR
        constexpr auto kScriptingCategory    = TEXT("Scripting.CoreCLR");
        constexpr auto kManagedRuntimeConfig = TEXT("AltinaEngine.Managed.runtimeconfig.json");
        constexpr auto kManagedAssembly      = TEXT("AltinaEngine.Managed.dll");
        constexpr auto kManagedType =
            TEXT("AltinaEngine.Managed.ManagedBootstrap, AltinaEngine.Managed");
        constexpr auto kManagedStartupMethod = TEXT("Startup");
        constexpr auto kManagedStartupDelegate =
            TEXT("AltinaEngine.Managed.ManagedStartupDelegate, AltinaEngine.Managed");

        struct FManagedPathResolve {
            Core::Utility::Filesystem::FPath mPath;
            bool                             mExists = false;
        };

        auto ToFString(const Core::Utility::Filesystem::FPath& path) -> Container::FString {
            return path.GetString();
        }

        auto ResolveManagedPath(const Core::Utility::Filesystem::FPath& exeDir,
            const TChar* fileName) -> FManagedPathResolve {
            using Core::Utility::Filesystem::FPath;
            FManagedPathResolve result{};
            if (fileName == nullptr || fileName[0] == static_cast<TChar>(0)) {
                return result;
            }

            const FPath filePart(fileName);

            auto        TryCandidate = [&](const FPath& root) -> bool {
                if (root.IsEmpty()) {
                    return false;
                }
                const auto candidate = root / filePart.ToView();
                if (candidate.Exists()) {
                    result.mPath   = candidate;
                    result.mExists = true;
                    return true;
                }
                return false;
            };

            if (TryCandidate(exeDir)) {
                return result;
            }

            if (!exeDir.IsEmpty()) {
                const auto parent = exeDir.ParentPath();
                if (TryCandidate(parent)) {
                    return result;
                }
            }

            const auto cwd = Core::Utility::Filesystem::GetCurrentWorkingDir();
            if (TryCandidate(cwd)) {
                return result;
            }

            result.mPath = exeDir.IsEmpty() ? filePart : (exeDir / filePart.ToView());
            return result;
        }
#endif
    } // namespace

    FEngineLoop::FEngineLoop(const FStartupParameters& InStartupParameters)
        : mStartupParameters(InStartupParameters) {}

    FEngineLoop::~FEngineLoop() = default;

    auto FEngineLoop::PreInit() -> bool {
        Core::Jobs::FJobSystem::RegisterGameThread();
        if (mApplication) {
            return true;
        }

        EnsureEngineReflectionRegistered();

        if (!mInputSystem) {
            mInputSystem = MakeUnique<Input::FInputSystem>();
        }

        if (!mAppMessageHandler && mInputSystem) {
            mAppMessageHandler = MakeUnique<Input::FInputMessageHandler>(*mInputSystem);
        }

        if (!mDebugGui) {
            using Core::Container::TPolymorphicDeleter;
            using DebugGui::IDebugGuiSystem;
            mDebugGui = TOwner<IDebugGuiSystem, TPolymorphicDeleter<IDebugGuiSystem>>(
                DebugGui::CreateDebugGuiSystem(),
                TPolymorphicDeleter<IDebugGuiSystem>(&DebugGui::DestroyDebugGuiSystem));
        }

#if AE_PLATFORM_WIN
        mApplication = MakeUniqueAs<Application::FApplication, Application::FWindowsApplication>(
            mStartupParameters);
#else
        LogError(TEXT("FEngineLoop PreInit failed: no platform application available."));
        return false;
#endif

        if (!mApplication) {
            LogError(TEXT("FEngineLoop PreInit failed: application allocation failed."));
            return false;
        }

        if (mAppMessageHandler) {
            mApplication->RegisterMessageHandler(mAppMessageHandler.Get());
        }

        // Demo/client window size overrides (if present) must be applied before window creation.
        {
            const auto& config = Core::Utility::EngineConfig::GetGlobalConfig();
            auto        props  = mApplication->GetWindowProperties();

            const u32   resolutionX = config.GetUint32(TEXT("GameClient/ResolutionX"));
            const u32   resolutionY = config.GetUint32(TEXT("GameClient/ResolutionY"));
            if (resolutionX > 0U) {
                props.mWidth = resolutionX;
            }
            if (resolutionY > 0U) {
                props.mHeight = resolutionY;
            }

            mApplication->SetWindowProperties(props);
        }

        mApplication->Initialize();
        if (!mApplication->IsRunning()) {
            LogError(TEXT("FEngineLoop PreInit failed: application did not start."));
            return false;
        }

        mIsRunning = true;
        return true;
    }

    auto FEngineLoop::Init() -> bool {
        if (!mApplication) {
            LogError(TEXT("FEngineLoop Init failed: application is not initialized."));
            return false;
        }

        if (mRhiContext) {
            return true;
        }

        if (!mAssetReady) {
            mAssetManager.SetRegistry(&mAssetRegistry);
            mAssetManager.RegisterLoader(&mAudioLoader);
            mAssetManager.RegisterLoader(&mMaterialLoader);
            mAssetManager.RegisterLoader(&mModelLoader);
            mAssetManager.RegisterLoader(&mMeshLoader);
            mAssetManager.RegisterLoader(&mScriptLoader);
            mAssetManager.RegisterLoader(&mShaderLoader);
            mAssetManager.RegisterLoader(&mTexture2DLoader);
            mAssetManager.RegisterLoader(&mCubeMapLoader);
            GameScene::FScriptComponent::SetAssetManager(&mAssetManager);
            BindMeshMaterialConverter(mAssetRegistry, mAssetManager);
            BindStaticMeshConverter(mAssetRegistry, mAssetManager);
            BindSkyCubeConverter(mAssetManager);
            mAssetReady = true;
        }

        if (!LoadDemoAssetRegistry()) {
            LogWarning(TEXT("Demo asset registry not loaded."));
        }

#if AE_PLATFORM_WIN
        mRhiContext = MakeUniqueAs<Rhi::FRhiContext, Rhi::FRhiD3D11Context>();
#else
        mRhiContext = MakeUniqueAs<Rhi::FRhiContext, Rhi::FRhiMockContext>();
#endif

        if (!mRhiContext) {
            LogError(TEXT("FEngineLoop Init failed: RHI context allocation failed."));
            return false;
        }

        Rhi::FRhiInitDesc initDesc{};
        initDesc.mAppName.Assign(TEXT("AltinaEngine"));
#if AE_PLATFORM_WIN
        initDesc.mBackend          = Rhi::ERhiBackend::DirectX11;
        initDesc.mEnableDebugLayer = true;
#endif

        Rhi::FRhiDeviceDesc deviceDesc{};
        deviceDesc.mEnableDebugLayer    = initDesc.mEnableDebugLayer;
        deviceDesc.mEnableGpuValidation = initDesc.mEnableGpuValidation;

        mRhiDevice = Rhi::RHIInit(*mRhiContext, initDesc, deviceDesc);
        if (!mRhiDevice) {
            LogError(TEXT("FEngineLoop Init failed: RHIInit failed."));
            return false;
        }

        Rendering::InitCommonRendererResource();

        auto* window = mApplication->GetMainWindow();
        if (!window) {
            LogError(TEXT("FEngineLoop Init failed: main window is missing."));
            return false;
        }

        const auto            extent = window->GetSize();
        Rhi::FRhiViewportDesc viewportDesc{};
        viewportDesc.mDebugName.Assign(TEXT("MainViewport"));
        viewportDesc.mWidth        = extent.mWidth;
        viewportDesc.mHeight       = extent.mHeight;
        viewportDesc.mNativeHandle = window->GetNativeHandle();
        mMainViewport              = Rhi::RHICreateViewport(viewportDesc);
        if (!mMainViewport) {
            LogError(TEXT("FEngineLoop Init failed: viewport creation failed."));
            return false;
        }

        mViewportWidth  = extent.mWidth;
        mViewportHeight = extent.mHeight;

        if (!mRenderingThread) {
            mRenderingThread = MakeUnique<RenderCore::FRenderingThread>();
        }
        if (mRenderingThread && !mRenderingThread->IsRunning()) {
            mRenderingThread->Start();
        }

#if defined(AE_ENABLE_SCRIPTING_CORECLR) && AE_ENABLE_SCRIPTING_CORECLR
        if (!mScriptSystem) {
            mScriptSystem = MakeUnique<Scripting::CoreCLR::FScriptSystem>();
        }
        if (mScriptSystem) {
            gScriptWorldManager        = &mEngineRuntime.GetWorldManager();
            gMainWindowForManagedTitle = window;
            Scripting::CoreCLR::SetTransformAccess(&GetScriptWorldTranslation,
                &SetScriptWorldTranslation, &GetScriptLocalTranslation, &SetScriptLocalTranslation,
                &GetScriptWorldRotation, &SetScriptWorldRotation, &GetScriptLocalRotation,
                &SetScriptLocalRotation);
            Scripting::CoreCLR::SetWindowTitleAccess(&SetScriptWindowTitleUtf8);
            Scripting::FScriptRuntimeConfig runtimeConfig{};
            const auto                      exeDir =
                Core::Utility::Filesystem::FPath(Core::Platform::GetExecutableDir());
            const auto runtimePath = ResolveManagedPath(exeDir, kManagedRuntimeConfig);
            if (runtimePath.mPath.IsEmpty()) {
                runtimeConfig.mRuntimeConfigPath.Assign(kManagedRuntimeConfig);
            } else {
                runtimeConfig.mRuntimeConfigPath = ToFString(runtimePath.mPath);
            }
            if (!runtimePath.mExists) {
                LogWarningCat(kScriptingCategory, TEXT("Managed runtime config not found at {}."),
                    runtimeConfig.mRuntimeConfigPath.ToView());
            }

            Scripting::CoreCLR::FManagedRuntimeConfig managedConfig{};
            const auto assemblyPath = ResolveManagedPath(exeDir, kManagedAssembly);
            if (assemblyPath.mPath.IsEmpty()) {
                managedConfig.mAssemblyPath.Assign(kManagedAssembly);
            } else {
                managedConfig.mAssemblyPath = ToFString(assemblyPath.mPath);
            }
            if (!assemblyPath.mExists) {
                LogWarningCat(kScriptingCategory, TEXT("Managed assembly not found at {}."),
                    managedConfig.mAssemblyPath.ToView());
            }
            managedConfig.mTypeName.Assign(kManagedType);
            managedConfig.mMethodName.Assign(kManagedStartupMethod);
            managedConfig.mDelegateTypeName.Assign(kManagedStartupDelegate);

            const bool scriptingReady =
                mScriptSystem->Initialize(runtimeConfig, managedConfig, mInputSystem.Get());
            if (!scriptingReady) {
                LogWarningCat(kScriptingCategory, TEXT("Managed scripting runtime init failed."));
            } else {
                LogInfoCat(kScriptingCategory, TEXT("Managed scripting runtime initialized."));
            }
        }
#endif

        return true;
    }

    void FEngineLoop::Tick(float InDeltaTime) {
        if (!mIsRunning) {
            return;
        }

        mLastDeltaTimeSeconds = InDeltaTime;
        Core::Jobs::FJobSystem::ProcessGameThreadJobs();

        if (mInputSystem) {
            mInputSystem->ClearFrameState();
        }

        if (mApplication) {
            mApplication->Tick(InDeltaTime);
            if (!mApplication->IsRunning()) {
                mIsRunning = false;
            }
        }

        if (!mIsRunning || !mRhiDevice) {
            return;
        }

        if (auto* world = mEngineRuntime.GetWorldManager().GetActiveWorld()) {
            world->Tick(InDeltaTime);
        }

        Draw();
    }

    void FEngineLoop::Draw() {
        u32  width        = 0U;
        u32  height       = 0U;
        bool shouldResize = false;

        if (mApplication) {
            auto* window = mApplication->GetMainWindow();
            if (window != nullptr) {
                const auto extent = window->GetSize();
                width             = extent.mWidth;
                height            = extent.mHeight;
                if (width > 0U && height > 0U) {
                    if (width != mViewportWidth || height != mViewportHeight) {
                        mViewportWidth  = width;
                        mViewportHeight = height;
                        shouldResize    = true;
                    }
                }
            }
        }

        const u64  frameIndex   = ++mFrameIndex;
        auto       device       = mRhiDevice;
        auto       viewport     = mMainViewport;
        auto       callback     = mRenderCallback;
        auto*      debugGui     = mDebugGui.Get();
        const auto rendererType = Rendering::GetRendererTypeSetting();
        if (rendererType == Rendering::ERendererType::Deferred) {
            mMaterialCache.SetDefaultTemplate(
                Rendering::FBasicDeferredRenderer::GetDefaultMaterialTemplate());
        }

        Engine::FRenderScene                   renderScene;
        TVector<RenderCore::Render::FDrawList> drawLists;
        TVector<RenderCore::Render::FDrawList> shadowDrawLists;

        if (width > 0U && height > 0U) {
            if (auto* world = mEngineRuntime.GetWorldManager().GetActiveWorld()) {
                Engine::FSceneViewBuildParams viewParams{};
                viewParams.ViewRect = RenderCore::View::FViewRect{ 0, 0, width, height };
                viewParams.RenderTargetExtent =
                    RenderCore::View::FRenderTargetExtent2D{ width, height };
                viewParams.FrameIndex          = frameIndex;
                viewParams.DeltaTimeSeconds    = mLastDeltaTimeSeconds;
                viewParams.ViewTarget.Type     = Engine::FSceneView::ETargetType::Viewport;
                viewParams.ViewTarget.Viewport = viewport.Get();

                Engine::FSceneViewBuilder viewBuilder;
                viewBuilder.Build(*world, viewParams, renderScene);

                if (!renderScene.Views.IsEmpty()) {
                    Engine::FSceneBatchBuilder     batchBuilder;
                    Engine::FSceneBatchBuildParams batchParams{};
                    batchParams.bAllowInstancing = false;
                    drawLists.Resize(renderScene.Views.Size());
                    shadowDrawLists.Resize(renderScene.Views.Size());
                    for (usize i = 0; i < renderScene.Views.Size(); ++i) {
                        batchBuilder.Build(renderScene, renderScene.Views[i], batchParams,
                            mMaterialCache, drawLists[i]);

                        // Shadow pass draw list (Directional CSM).
                        Engine::FSceneBatchBuildParams shadowParams = batchParams;
                        shadowParams.Pass = RenderCore::EMaterialPass::ShadowPass;
                        batchBuilder.Build(renderScene, renderScene.Views[i], shadowParams,
                            mMaterialCache, shadowDrawLists[i]);
                    }
                    for (auto& drawList : drawLists) {
                        for (const auto& batch : drawList.Batches) {
                            if (batch.Material != nullptr) {
                                auto* material = const_cast<RenderCore::FMaterial*>(batch.Material);
                                mMaterialCache.PrepareMaterialForRendering(*material);
                            }
                        }
                    }
                    for (auto& drawList : shadowDrawLists) {
                        for (const auto& batch : drawList.Batches) {
                            if (batch.Material != nullptr) {
                                auto* material = const_cast<RenderCore::FMaterial*>(batch.Material);
                                mMaterialCache.PrepareMaterialForRendering(*material);
                            }
                        }
                    }
                }
            }
        }

        u32 totalBatches = 0U;
        for (const auto& drawList : drawLists) {
            totalBatches += static_cast<u32>(drawList.Batches.Size());
        }

        if (mDebugGui && mInputSystem) {
            DebugGui::FDebugGuiExternalStats stats{};
            stats.FrameIndex      = frameIndex;
            stats.ViewCount       = static_cast<u32>(renderScene.Views.Size());
            stats.SceneBatchCount = totalBatches;
            mDebugGui->SetExternalStats(stats);
            mDebugGui->TickGameThread(*mInputSystem.Get(), mLastDeltaTimeSeconds, width, height);
        }
        // LogInfo(TEXT("Scene Batches: {} (Views: {})"), totalBatches,
        //     static_cast<u32>(renderScene.Views.Size()));

        // LogInfo(TEXT("GameThread Frame {}"), frameIndex);

        auto* assetRegistry = &mAssetRegistry;
        auto* assetManager  = &mAssetManager;

        auto  handle = RenderCore::EnqueueRenderTask(Container::FString(TEXT("RenderFrame")),
             [device, viewport, callback, debugGui, frameIndex, width, height, shouldResize,
                renderScene = Move(renderScene), drawLists = Move(drawLists),
                shadowDrawLists = Move(shadowDrawLists), rendererType, assetRegistry,
                assetManager]() mutable -> void {
                if (!device)
                    return;

                device->BeginFrame(frameIndex);

                if (viewport && width > 0U && height > 0U) {
                    if (shouldResize) {
                        viewport->Resize(width, height);
                    }

                    if (!renderScene.Views.IsEmpty()) {
                        SendSceneRenderingRequest(*device, viewport.Get(), renderScene, drawLists,
                             shadowDrawLists, rendererType, assetRegistry, assetManager);
                    }

                    if (callback) {
                        callback(*device, *viewport, width, height);
                    }

                    if (debugGui) {
                        debugGui->RenderRenderThread(*device, *viewport);
                    }

                    const auto queue = device->GetQueue(Rhi::ERhiQueueType::Graphics);
                    if (queue) {
                        Rhi::FRhiPresentInfo presentInfo{};
                        presentInfo.mViewport     = viewport.Get();
                        presentInfo.mSyncInterval = 1U;
                        queue->Present(presentInfo);
                    }
                }

                device->EndFrame();

                // LogInfo(TEXT("RenderThread Frame {}"), frameIndex);
            });
        if (handle.IsValid()) {
            mPendingRenderFrames.Push(handle);
            EnforceRenderLag(GetRenderThreadLagFrames());
        }
    }

    void FEngineLoop::Exit() {
        mIsRunning = false;

        FlushRenderFrames();
        if (mRenderingThread) {
            mRenderingThread->Stop();
            mRenderingThread.Reset();
        }

        if (mDebugGui) {
            mDebugGui.Reset();
        }

        if (mAssetReady) {
            mMaterialCache.Clear();
            mAssetManager.ClearCache();
            mAssetManager.UnregisterLoader(&mTexture2DLoader);
            mAssetManager.UnregisterLoader(&mShaderLoader);
            mAssetManager.UnregisterLoader(&mScriptLoader);
            mAssetManager.UnregisterLoader(&mMeshLoader);
            mAssetManager.UnregisterLoader(&mModelLoader);
            mAssetManager.UnregisterLoader(&mMaterialLoader);
            mAssetManager.UnregisterLoader(&mAudioLoader);
            mAssetManager.SetRegistry(nullptr);
            GameScene::FScriptComponent::SetAssetManager(nullptr);
            GameScene::FMeshMaterialComponent::AssetToRenderMaterialConverter = {};
            GameScene::FStaticMeshFilterComponent::AssetToStaticMeshConverter = {};
            mAssetReady                                                       = false;
        }

        if (mMainViewport) {
            mMainViewport->SetDeleteQueue(nullptr);
            mMainViewport.Reset();
        }
        if (mRhiDevice) {
            mRhiDevice->FlushResourceDeleteQueue();
        }
        mRhiDevice.Reset();
        if (mRhiContext) {
            Rhi::RHIExit(*mRhiContext);
            mRhiContext.Reset();
        }

        if (mApplication && mAppMessageHandler) {
            mApplication->UnregisterMessageHandler(mAppMessageHandler.Get());
        }

        if (mApplication) {
            mApplication->Shutdown();
            mApplication.Reset();
        }

        if (mAppMessageHandler) {
            mAppMessageHandler.Reset();
        }

        if (mScriptSystem) {
            mScriptSystem->Shutdown();
            mScriptSystem.Reset();
        }
#if defined(AE_ENABLE_SCRIPTING_CORECLR) && AE_ENABLE_SCRIPTING_CORECLR
        Scripting::CoreCLR::SetTransformAccess(
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        Scripting::CoreCLR::SetWindowTitleAccess(nullptr);
        gScriptWorldManager        = nullptr;
        gMainWindowForManagedTitle = nullptr;
#endif

        if (mInputSystem) {
            mInputSystem.Reset();
        }
    }

    void FEngineLoop::SetRenderCallback(FRenderCallback callback) {
        FlushRenderFrames();
        mRenderCallback = Move(callback);
    }

    auto FEngineLoop::GetInputSystem() noexcept -> Input::FInputSystem* {
        return mInputSystem.Get();
    }

    auto FEngineLoop::GetInputSystem() const noexcept -> const Input::FInputSystem* {
        return mInputSystem.Get();
    }

    auto FEngineLoop::GetMainWindow() noexcept -> Application::FPlatformWindow* {
        return mApplication ? mApplication->GetMainWindow() : nullptr;
    }

    auto FEngineLoop::GetWorldManager() noexcept -> GameScene::FWorldManager& {
        return mEngineRuntime.GetWorldManager();
    }

    auto FEngineLoop::GetWorldManager() const noexcept -> const GameScene::FWorldManager& {
        return mEngineRuntime.GetWorldManager();
    }

    auto FEngineLoop::GetAssetRegistry() noexcept -> Asset::FAssetRegistry& {
        return mAssetRegistry;
    }

    auto FEngineLoop::GetAssetRegistry() const noexcept -> const Asset::FAssetRegistry& {
        return mAssetRegistry;
    }

    auto FEngineLoop::GetAssetManager() noexcept -> Asset::FAssetManager& { return mAssetManager; }

    auto FEngineLoop::GetAssetManager() const noexcept -> const Asset::FAssetManager& {
        return mAssetManager;
    }

    auto FEngineLoop::GetDebugGui() noexcept -> DebugGui::IDebugGuiSystem* {
        return mDebugGui.Get();
    }

    auto FEngineLoop::GetDebugGui() const noexcept -> const DebugGui::IDebugGuiSystem* {
        return mDebugGui.Get();
    }

    auto FEngineLoop::LoadDemoAssetRegistry() -> bool {
        const auto baseDir = Core::Platform::GetExecutableDir();
        if (baseDir.IsEmptyString()) {
            return false;
        }

        Container::FString registryPath = baseDir;
        registryPath.Append(TEXT("/Assets/Registry/AssetRegistry.json"));
        if (!Core::Platform::IsPathExist(registryPath)) {
            return false;
        }

        if (!mAssetRegistry.LoadFromJsonFile(registryPath)) {
            return false;
        }

        const auto assetRoot =
            Core::Utility::Filesystem::FPath(registryPath).ParentPath().ParentPath();
        if (!Core::Utility::Filesystem::SetCurrentWorkingDir(assetRoot)) {
            const auto rootText = ToFString(assetRoot);
            LogWarning(TEXT("Failed to set asset root to {}."), rootText.ToView());
        }
        return true;
    }

    void FEngineLoop::FlushRenderFrames() {
        while (!mPendingRenderFrames.IsEmpty()) {
            auto handle = mPendingRenderFrames.Front();
            mPendingRenderFrames.Pop();
            Core::Jobs::FJobSystem::Wait(handle);
        }
    }

    void FEngineLoop::EnforceRenderLag(u32 maxLagFrames) {
        while (mPendingRenderFrames.Size() > maxLagFrames) {
            auto handle = mPendingRenderFrames.Front();
            mPendingRenderFrames.Pop();
            Core::Jobs::FJobSystem::Wait(handle);
        }
    }
} // namespace AltinaEngine::Launch
