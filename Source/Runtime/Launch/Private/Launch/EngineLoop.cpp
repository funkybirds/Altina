#include "Launch/EngineLoop.h"

#include "Input/InputMessageHandler.h"
#include "Input/InputSystem.h"
#include "Engine/EngineReflection.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/ScriptComponent.h"
#include "Engine/Runtime/SceneBatching.h"
#include "Engine/Runtime/SceneView.h"
#include "Rendering/BasicDeferredRenderer.h"
#include "Atmosphere/AtmosphereSystem.h"
#include "Rendering/BasicForwardRenderer.h"
#include "Rendering/CommonRendererResource.h"
#include "Rendering/PostProcess/PostProcess.h"
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
#include "FrameGraph/FrameGraphExecutor.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiCommandContext.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiStructs.h"
#include "Rhi/Command/RhiCmdContextAdapter.h"
#include "Reflection/Reflection.h"
#include "Utility/Assert.h"

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
    #include "RhiVulkan/RhiVulkanContext.h"
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
using AltinaEngine::Core::Utility::Assert;
using AltinaEngine::Core::Utility::DebugAssert;

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

        [[nodiscard]] auto ParseWindowDpiPolicy(const Container::FString& value) noexcept
            -> Application::EWindowDpiPolicy {
            const auto view = value.ToView();
            if (view == Container::FStringView(TEXT("PhysicalFixed"))) {
                return Application::EWindowDpiPolicy::PhysicalFixed;
            }
            return Application::EWindowDpiPolicy::LogicalFixed;
        }

        void ResolveLogicalWindowSize(const Core::Utility::EngineConfig::FConfigCollection& config,
            u32& outWidth, u32& outHeight) {
            u32 logicalWidth  = config.GetUint32(TEXT("Window/LogicalWidth"));
            u32 logicalHeight = config.GetUint32(TEXT("Window/LogicalHeight"));

            if (logicalWidth == 0U) {
                logicalWidth = config.GetUint32(TEXT("GameClient/ResolutionX"));
            }
            if (logicalHeight == 0U) {
                logicalHeight = config.GetUint32(TEXT("GameClient/ResolutionY"));
            }
            if (logicalWidth == 0U) {
                logicalWidth = 1280U;
            }
            if (logicalHeight == 0U) {
                logicalHeight = 720U;
            }

            outWidth  = logicalWidth;
            outHeight = logicalHeight;
        }

        [[nodiscard]] auto ClampInternalRenderScale(f32 value) noexcept -> f32 {
            if (value <= 0.01f) {
                return 1.0f;
            }
            if (value > 4.0f) {
                return 4.0f;
            }
            return value;
        }

        [[nodiscard]] auto ScaleRenderDimension(u32 value, f32 scale) noexcept -> u32 {
            if (value == 0U) {
                return 0U;
            }
            const f32 scaled = static_cast<f32>(value) * scale;
            u32       out    = static_cast<u32>(scaled + 0.5f);
            if (out == 0U) {
                out = 1U;
            }
            return out;
        }

#if defined(AE_ENABLE_SCRIPTING_CORECLR) && AE_ENABLE_SCRIPTING_CORECLR
        GameScene::FWorldManager*     gScriptWorldManager        = nullptr;
        Application::FPlatformWindow* gMainWindowForManagedTitle = nullptr;

        auto ToFStringFromUtf8Bytes(const char* data) -> Core::Container::FString {
            using Container::FString;
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
            return { data, length };
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
                    h = HashCombine(h, HashUuid(view.Target.Texture.mUuid));
                    h = HashCombine(h, static_cast<u64>(view.Target.Texture.mType));
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
            RenderCore::FFrameGraphExecutor executor(device);
            executor.Execute(graph);
        }

        struct FSceneRenderCaches {
            Asset::FAssetHandle                                SkyCubeAsset{};
            GameScene::FSkyCubeComponent::FSkyCubeRhiResources SkyCubeRhi{};
            bool                                               HasSkyCubeRhi = false;

            Asset::FAssetHandle                                SkyIrradianceAsset{};
            GameScene::FSkyCubeComponent::FSkyCubeRhiResources SkyIrradianceRhi{};
            bool                                               HasSkyIrradianceRhi = false;

            Asset::FAssetHandle                                SkySpecularAsset{};
            GameScene::FSkyCubeComponent::FSkyCubeRhiResources SkySpecularRhi{};
            bool                                               HasSkySpecularRhi = false;

            Asset::FAssetHandle                                BrdfLutAsset{};
            Rhi::FRhiTextureRef                                BrdfLutTexture{};
            bool                                               HasBrdfLutTexture = false;
        };

        auto GetSceneRenderCaches() -> FSceneRenderCaches& {
            static FSceneRenderCaches caches{};
            return caches;
        }

        void ClearSceneRenderCaches() {
            auto& caches               = GetSceneRenderCaches();
            caches.SkyCubeAsset        = {};
            caches.SkyCubeRhi          = {};
            caches.HasSkyCubeRhi       = false;
            caches.SkyIrradianceAsset  = {};
            caches.SkyIrradianceRhi    = {};
            caches.HasSkyIrradianceRhi = false;
            caches.SkySpecularAsset    = {};
            caches.SkySpecularRhi      = {};
            caches.HasSkySpecularRhi   = false;
            caches.BrdfLutAsset        = {};
            caches.BrdfLutTexture.Reset();
            caches.HasBrdfLutTexture = false;
        }

        void EnsureFallbackBrdfLutTexture(Rhi::FRhiTextureRef& outTexture, bool& outHasTexture) {
            if (outHasTexture && outTexture) {
                return;
            }

            auto* rhiDevice = Rhi::RHIGetDevice();
            if (rhiDevice == nullptr) {
                return;
            }

            constexpr u32        kSize = 128U;
            Rhi::FRhiTextureDesc texDesc{};
            texDesc.mDebugName.Assign(TEXT("IBL.BrdfLut.Fallback"));
            texDesc.mDimension = Rhi::ERhiTextureDimension::Tex2D;
            texDesc.mWidth     = kSize;
            texDesc.mHeight    = kSize;
            texDesc.mMipLevels = 1U;
            texDesc.mFormat    = Rhi::ERhiFormat::R8G8B8A8Unorm;
            texDesc.mBindFlags = Rhi::ERhiTextureBindFlags::ShaderResource;

            outTexture = Rhi::RHICreateTexture(texDesc);
            if (!outTexture) {
                return;
            }

            Core::Container::TVector<u8> pixels{};
            pixels.Reserve(static_cast<usize>(kSize) * static_cast<usize>(kSize) * 4U);
            for (u32 y = 0U; y < kSize; ++y) {
                const f32 roughness = static_cast<f32>(y) / static_cast<f32>(kSize - 1U);
                for (u32 x = 0U; x < kSize; ++x) {
                    const f32 ndv = static_cast<f32>(x) / static_cast<f32>(kSize - 1U);
                    const f32 scale =
                        Core::Math::Clamp(0.04f + (1.0f - roughness) * 0.5f * ndv, 0.0f, 1.0f);
                    const f32 bias =
                        Core::Math::Clamp(roughness * 0.25f + (1.0f - ndv) * 0.1f, 0.0f, 1.0f);
                    pixels.PushBack(static_cast<u8>(scale * 255.0f));
                    pixels.PushBack(static_cast<u8>(bias * 255.0f));
                    pixels.PushBack(0U);
                    pixels.PushBack(255U);
                }
            }

            rhiDevice->UpdateTextureSubresource(outTexture.Get(), Rhi::FRhiTextureSubresource{},
                pixels.Data(), kSize * 4U, kSize * 4U * kSize);
            outHasTexture = true;
        }

        void SendSceneRenderingRequest(Rhi::FRhiDevice& device, Rhi::FRhiViewport* defaultViewport,
            Engine::FRenderScene& scene, const TVector<RenderCore::Render::FDrawList>& drawLists,
            const TVector<RenderCore::Render::FDrawList>& shadowDrawLists,
            Rendering::ERendererType rendererType, const Asset::FAssetRegistry* assetRegistry,
            Asset::FAssetManager* assetManager, Rhi::FRhiTexture* primaryViewOutputOverride) {
            DebugAssert(defaultViewport != nullptr, TEXT("Launch.EngineLoop"),
                "SendSceneRenderingRequest: default viewport is null.");
            if (scene.Views.IsEmpty()) {
                DebugAssert(false, TEXT("Launch.EngineLoop"),
                    "SendSceneRenderingRequest: scene has no views.");
                return;
            }
            DebugAssert(drawLists.Size() == scene.Views.Size(), TEXT("Launch.EngineLoop"),
                "SendSceneRenderingRequest: draw list/view size mismatch (drawLists={}, views={}).",
                static_cast<u32>(drawLists.Size()), static_cast<u32>(scene.Views.Size()));
            DebugAssert(shadowDrawLists.Size() == scene.Views.Size(), TEXT("Launch.EngineLoop"),
                "SendSceneRenderingRequest: shadow draw list/view size mismatch (shadowDrawLists={}, views={}).",
                static_cast<u32>(shadowDrawLists.Size()), static_cast<u32>(scene.Views.Size()));

            // Cache skybox/IBL GPU resources on the render thread to avoid re-uploading.
            auto&             cache                = GetSceneRenderCaches();
            auto&             sSkyCubeAsset        = cache.SkyCubeAsset;
            auto&             sSkyCubeRhi          = cache.SkyCubeRhi;
            bool&             sHasSkyCubeRhi       = cache.HasSkyCubeRhi;
            auto&             sSkyIrradianceAsset  = cache.SkyIrradianceAsset;
            auto&             sSkyIrradianceRhi    = cache.SkyIrradianceRhi;
            bool&             sHasSkyIrradianceRhi = cache.HasSkyIrradianceRhi;
            auto&             sSkySpecularAsset    = cache.SkySpecularAsset;
            auto&             sSkySpecularRhi      = cache.SkySpecularRhi;
            bool&             sHasSkySpecularRhi   = cache.HasSkySpecularRhi;
            auto&             sBrdfLutAsset        = cache.BrdfLutAsset;
            auto&             sBrdfLutTexture      = cache.BrdfLutTexture;
            bool&             sHasBrdfLutTexture   = cache.HasBrdfLutTexture;

            Rhi::FRhiTexture* skyCubeTexture                   = nullptr;
            Rhi::FRhiBuffer*  atmosphereParamsBuffer           = nullptr;
            Rhi::FRhiTexture* atmosphereTransmittanceLut       = nullptr;
            Rhi::FRhiTexture* atmosphereIrradianceLut          = nullptr;
            Rhi::FRhiTexture* atmosphereScatteringLut          = nullptr;
            Rhi::FRhiTexture* atmosphereSingleMieScatteringLut = nullptr;
            bool              bHasSkyCube                      = false;
            bool              bHasAtmosphereSky                = false;
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
                if (baseDesc != nullptr && !baseDesc->mVirtualPath.IsEmpty()) {
                    auto makeDerivedPath = [&](const TChar* suffix) -> Container::FString {
                        Container::FString out = baseDesc->mVirtualPath;
                        out.Append(TEXT("/"));
                        out.Append(suffix);
                        return out;
                    };

                    const auto irradiancePath = makeDerivedPath(TEXT("irradiance"));
                    const auto specularPath   = makeDerivedPath(TEXT("specular"));
                    const auto brdfPath       = makeDerivedPath(TEXT("brdf_lut"));

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
                            && specDesc->mHandle.mType == Asset::EAssetType::CubeMap
                            && specDesc->mCubeMap.MipCount > 0U) {
                            specularMaxLod = static_cast<float>(specDesc->mCubeMap.MipCount - 1U);
                        }
                    }

                    if (brdfHandle.IsValid()) {
                        const auto* brdfDesc = assetRegistry->GetDesc(brdfHandle);
                        if (brdfDesc == nullptr
                            || brdfDesc->mHandle.mType != Asset::EAssetType::Texture2D) {
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

            if (scene.bHasPbrSky) {
                Rendering::Atmosphere::FAtmosphereSkyDesc atmosphereDesc{};
                atmosphereDesc.mRayleighScattering    = scene.PbrSky.RayleighScattering;
                atmosphereDesc.mRayleighScaleHeightKm = scene.PbrSky.RayleighScaleHeightKm;
                atmosphereDesc.mMieScattering         = scene.PbrSky.MieScattering;
                atmosphereDesc.mMieAbsorption         = scene.PbrSky.MieAbsorption;
                atmosphereDesc.mMieScaleHeightKm      = scene.PbrSky.MieScaleHeightKm;
                atmosphereDesc.mMieAnisotropy         = scene.PbrSky.MieAnisotropy;
                atmosphereDesc.mOzoneAbsorption       = scene.PbrSky.OzoneAbsorption;
                atmosphereDesc.mOzoneCenterHeightKm   = scene.PbrSky.OzoneCenterHeightKm;
                atmosphereDesc.mOzoneThicknessKm      = scene.PbrSky.OzoneThicknessKm;
                atmosphereDesc.mGroundAlbedo          = scene.PbrSky.GroundAlbedo;
                atmosphereDesc.mSolarTint             = scene.PbrSky.SolarTint;
                atmosphereDesc.mSolarIlluminance      = scene.PbrSky.SolarIlluminance;
                atmosphereDesc.mSunAngularRadius      = scene.PbrSky.SunAngularRadius;
                atmosphereDesc.mPlanetRadiusKm        = scene.PbrSky.PlanetRadiusKm;
                atmosphereDesc.mAtmosphereHeightKm    = scene.PbrSky.AtmosphereHeightKm;
                atmosphereDesc.mViewHeightKm          = scene.PbrSky.ViewHeightKm;
                atmosphereDesc.mExposure              = scene.PbrSky.Exposure;
                atmosphereDesc.mVersion               = scene.PbrSky.Version;

                Core::Math::FVector3f sunDirection(0.4f, 0.6f, 0.7f);
                if (scene.Lights.mHasMainDirectionalLight) {
                    sunDirection = scene.Lights.mMainDirectionalLight.mDirectionWS;
                }

                const auto* atmosphereResources =
                    Rendering::Atmosphere::FAtmosphereSystem::Get().EnsureSkyResources(
                        atmosphereDesc, sunDirection);
                if (atmosphereResources != nullptr && atmosphereResources->IsValid()) {
                    atmosphereParamsBuffer     = atmosphereResources->mParamsBuffer.Get();
                    atmosphereTransmittanceLut = atmosphereResources->mTransmittanceLut.Get();
                    atmosphereIrradianceLut    = atmosphereResources->mIrradianceLut.Get();
                    atmosphereScatteringLut    = atmosphereResources->mScatteringLut.Get();
                    atmosphereSingleMieScatteringLut =
                        atmosphereResources->mSingleMieScatteringLut.Get();
                    bHasAtmosphereSky = (atmosphereParamsBuffer != nullptr)
                        && (atmosphereTransmittanceLut != nullptr)
                        && (atmosphereScatteringLut != nullptr)
                        && (atmosphereSingleMieScatteringLut != nullptr);
                    skyIrradiance  = nullptr;
                    skySpecular    = nullptr;
                    brdfLut        = nullptr;
                    specularMaxLod = 0.0f;
                    bHasSkyIbl     = false;
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
                    DebugAssert(false, TEXT("Launch.EngineLoop"),
                        "SendSceneRenderingRequest: scene view {} is invalid.",
                        static_cast<u32>(i));
                    continue;
                }

                auto* outputTarget = (i == 0U && primaryViewOutputOverride != nullptr)
                    ? primaryViewOutputOverride
                    : ResolveViewOutputTarget(view, defaultViewport);
                if (outputTarget == nullptr) {
                    LogWarningCat(TEXT("Launch.EngineLoop"),
                        "SendSceneRenderingRequest: skip view {} because output target is null (targetType={}, viewViewport={}, fallbackViewport={}).",
                        static_cast<u32>(i), static_cast<u32>(view.Target.Type),
                        static_cast<int>(view.Target.Viewport != nullptr),
                        static_cast<int>(defaultViewport != nullptr));
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
                viewContext.OutputTarget = outputTarget;
                const auto* swapchainBackBuffer =
                    (defaultViewport != nullptr) ? defaultViewport->GetBackBuffer() : nullptr;
                viewContext.OutputFinalState                 = (outputTarget == swapchainBackBuffer)
                                    ? Rhi::ERhiResourceState::Present
                                    : Rhi::ERhiResourceState::ShaderResource;
                viewContext.Lights                           = &scene.Lights;
                viewContext.SkyCubeTexture                   = skyCubeTexture;
                viewContext.bHasSkyCube                      = bHasSkyCube;
                viewContext.AtmosphereParamsBuffer           = atmosphereParamsBuffer;
                viewContext.AtmosphereTransmittanceLut       = atmosphereTransmittanceLut;
                viewContext.AtmosphereIrradianceLut          = atmosphereIrradianceLut;
                viewContext.AtmosphereScatteringLut          = atmosphereScatteringLut;
                viewContext.AtmosphereSingleMieScatteringLut = atmosphereSingleMieScatteringLut;
                viewContext.bHasAtmosphereSky                = bHasAtmosphereSky;
                viewContext.SkyIrradianceCube                = skyIrradiance;
                viewContext.SkySpecularCube                  = skySpecular;
                viewContext.BrdfLutTexture                   = brdfLut;
                viewContext.SkySpecularMaxLod                = specularMaxLod;
                viewContext.bHasSkyIbl                       = bHasSkyIbl;
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
        constexpr auto kScriptingCategory = TEXT("Scripting.CoreCLR");
        // NOTE: hostfxr native-hosting does not accept "includedFrameworks" runtimeconfig files
        // produced by some self-contained publish flows. Keep a host-friendly runtimeconfig
        // alongside the executable and fall back to the build-generated one if needed.
        constexpr auto kManagedRuntimeConfigHost =
            TEXT("AltinaEngine.Managed.host.runtimeconfig.json");
        constexpr auto kManagedRuntimeConfigFallback =
            TEXT("AltinaEngine.Managed.runtimeconfig.json");
        constexpr auto kManagedAssembly = TEXT("AltinaEngine.Managed.dll");
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
            mDebugGui = DebugGui::CreateDebugGuiSystemOwner();
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

        // Window settings must be applied before window creation.
        {
            const auto& config = Core::Utility::EngineConfig::GetGlobalConfig();
            auto        props  = mApplication->GetWindowProperties();
            ResolveLogicalWindowSize(config, mWindowLogicalWidth, mWindowLogicalHeight);
            props.mWidth     = mWindowLogicalWidth;
            props.mHeight    = mWindowLogicalHeight;
            props.mDpiPolicy = ParseWindowDpiPolicy(config.GetString(TEXT("Window/DpiPolicy")));
            mRenderInternalScale =
                ClampInternalRenderScale(config.GetFloat32(TEXT("Render/InternalScale")));

            mApplication->SetWindowProperties(props);
            LogInfo(TEXT("Window config logical={}x{} dpiPolicy={} renderScale={}."), props.mWidth,
                props.mHeight, static_cast<u32>(props.mDpiPolicy), mRenderInternalScale);
        }

        mApplication->Initialize();
        if (!mApplication->IsRunning()) {
            LogError(TEXT("FEngineLoop PreInit failed: application did not start."));
            return false;
        }
        if (mInputSystem) {
            // Some startup paths may miss the initial WM_SETFOCUS message.
            // Prime keyboard focus so key events are not filtered out before first focus callback.
            mInputSystem->OnWindowFocusGained();
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
            mAssetManager.RegisterLoader(&mLevelLoader);
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
        const auto& config      = Core::Utility::EngineConfig::GetGlobalConfig();
        const bool  preferD3D11 = config.GetBool(TEXT("Rhi/PreferD3D11"));
        const auto  requestedRhi =
            preferD3D11 ? Rhi::ERhiBackend::DirectX11 : Rhi::ERhiBackend::Vulkan;

        auto backendName = [](Rhi::ERhiBackend backend) -> const TChar* {
            switch (backend) {
                case Rhi::ERhiBackend::Vulkan:
                    return TEXT("Vulkan");
                case Rhi::ERhiBackend::DirectX11:
                    return TEXT("DirectX11");
                default:
                    return TEXT("Unknown");
            }
        };

        auto tryInitializeBackend = [&](Rhi::ERhiBackend backend) -> bool {
            switch (backend) {
                case Rhi::ERhiBackend::Vulkan:
                    mRhiContext = MakeUniqueAs<Rhi::FRhiContext, Rhi::FRhiVulkanContext>();
                    break;
                case Rhi::ERhiBackend::DirectX11:
                    mRhiContext = MakeUniqueAs<Rhi::FRhiContext, Rhi::FRhiD3D11Context>();
                    break;
                default:
                    mRhiContext.Reset();
                    return false;
            }

            if (!mRhiContext) {
                return false;
            }

            Rhi::FRhiInitDesc initDesc{};
            initDesc.mAppName.Assign(TEXT("AltinaEngine"));
            initDesc.mBackend          = backend;
            initDesc.mEnableDebugLayer = true;

            Rhi::FRhiDeviceDesc deviceDesc{};
            deviceDesc.mEnableDebugLayer    = initDesc.mEnableDebugLayer;
            deviceDesc.mEnableGpuValidation = initDesc.mEnableGpuValidation;

            mRhiDevice = Rhi::RHIInit(*mRhiContext, initDesc, deviceDesc);
            return mRhiDevice != nullptr;
        };

        if (!tryInitializeBackend(requestedRhi)) {
            const auto fallbackRhi = (requestedRhi == Rhi::ERhiBackend::Vulkan)
                ? Rhi::ERhiBackend::DirectX11
                : Rhi::ERhiBackend::Vulkan;

            LogWarning(TEXT("RHI backend '{}' init failed. Fallback to '{}'."),
                backendName(requestedRhi), backendName(fallbackRhi));
            if (!tryInitializeBackend(fallbackRhi)) {
                LogError(
                    TEXT("FEngineLoop Init failed: RHIInit failed for '{}' and fallback '{}'."),
                    backendName(requestedRhi), backendName(fallbackRhi));
                return false;
            }
        }
#else
        mRhiContext = MakeUniqueAs<Rhi::FRhiContext, Rhi::FRhiMockContext>();
        if (!mRhiContext) {
            LogError(TEXT("FEngineLoop Init failed: RHI context allocation failed."));
            return false;
        }

        Rhi::FRhiInitDesc initDesc{};
        initDesc.mAppName.Assign(TEXT("AltinaEngine"));

        Rhi::FRhiDeviceDesc deviceDesc{};
        deviceDesc.mEnableDebugLayer    = initDesc.mEnableDebugLayer;
        deviceDesc.mEnableGpuValidation = initDesc.mEnableGpuValidation;

        mRhiDevice = Rhi::RHIInit(*mRhiContext, initDesc, deviceDesc);
        if (!mRhiDevice) {
            LogError(TEXT("FEngineLoop Init failed: RHIInit failed."));
            return false;
        }
#endif

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
            mScriptSystem = MakeUniqueAs<Scripting::CoreCLR::FScriptSystem,
                Scripting::CoreCLR::FScriptSystem>();
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
            FManagedPathResolve runtimePath = ResolveManagedPath(exeDir, kManagedRuntimeConfigHost);
            if (!runtimePath.mExists) {
                runtimePath = ResolveManagedPath(exeDir, kManagedRuntimeConfigFallback);
            }

            const auto& runtimePathValue     = runtimePath.mPath.IsEmpty()
                    ? Core::Utility::Filesystem::FPath(kManagedRuntimeConfigHost)
                    : runtimePath.mPath;
            runtimeConfig.mRuntimeConfigPath = ToFString(runtimePathValue);
            if (!runtimePath.mExists) {
                LogWarningCat(kScriptingCategory,
                    TEXT("Managed runtime config not found at {} (also tried fallback)."),
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

    auto FEngineLoop::BeginFrame(const FFrameContext& frameContext) -> bool {
        if (!mIsRunning) {
            return false;
        }

        mFrameActive          = true;
        mHostFrameIndex       = frameContext.FrameIndex;
        mLastDeltaTimeSeconds = frameContext.DeltaSeconds;
        Core::Jobs::FJobSystem::ProcessGameThreadJobs();

        if (mInputSystem) {
            mInputSystem->ClearFrameState();
        }

        if (mApplication) {
            mApplication->Tick(frameContext.DeltaSeconds);
            if (!mApplication->IsRunning()) {
                mIsRunning = false;
            }
        }

        if (!mIsRunning || !mRhiDevice) {
            mFrameActive = false;
            return false;
        }
        return true;
    }

    void FEngineLoop::TickSimulation(const FSimulationTick& tick) {
        if (!mFrameActive || !mIsRunning) {
            return;
        }

        const f32 scaledDelta = tick.DeltaSeconds * tick.TimeScale;
        if (auto* world = mEngineRuntime.GetWorldManager().GetActiveWorld()) {
            world->Tick(scaledDelta);
        }
    }

    void FEngineLoop::RenderFrame(const FRenderTick& tick) {
        if (!mFrameActive || !mIsRunning || !mRhiDevice) {
            return;
        }
        Draw(tick);
    }

    void FEngineLoop::EndFrame() { mFrameActive = false; }

    void FEngineLoop::Tick(float InDeltaTime) {
        FFrameContext frameContext{};
        frameContext.DeltaSeconds = InDeltaTime;
        frameContext.FrameIndex   = mFrameIndex + 1ULL;
        if (!BeginFrame(frameContext)) {
            return;
        }

        FSimulationTick simulationTick{};
        simulationTick.DeltaSeconds = InDeltaTime;
        TickSimulation(simulationTick);

        RenderFrame({});
        EndFrame();
    }

    void FEngineLoop::Draw(const FRenderTick& tick) {
        u32  windowWidth         = 0U;
        u32  windowHeight        = 0U;
        u32  renderWidth         = 0U;
        u32  renderHeight        = 0U;
        u32  windowLogicalWidth  = 0U;
        u32  windowLogicalHeight = 0U;
        u32  windowDpi           = 0U;
        f32  windowDpiScale      = 1.0f;
        bool shouldResize        = false;

        if (mApplication) {
            auto* window = mApplication->GetMainWindow();
            if (window != nullptr) {
                const auto extent     = window->GetSize();
                windowWidth           = extent.mWidth;
                windowHeight          = extent.mHeight;
                const auto properties = window->GetProperties();
                windowLogicalWidth    = properties.mWidth;
                windowLogicalHeight   = properties.mHeight;
                windowDpi             = properties.mDpi;
                windowDpiScale        = properties.mDpiScaling;
                if (windowWidth > 0U && windowHeight > 0U) {
                    if (windowWidth != mViewportWidth || windowHeight != mViewportHeight) {
                        mViewportWidth  = windowWidth;
                        mViewportHeight = windowHeight;
                        shouldResize    = true;
                    }
                }
            }
        }

        renderWidth  = windowWidth;
        renderHeight = windowHeight;
        if (tick.RenderWidth > 0U && tick.RenderHeight > 0U) {
            renderWidth  = tick.RenderWidth;
            renderHeight = tick.RenderHeight;
        }

        if (renderWidth > 0U && renderHeight > 0U) {
            const f32 safeScale = ClampInternalRenderScale(mRenderInternalScale);
            renderWidth         = ScaleRenderDimension(renderWidth, safeScale);
            renderHeight        = ScaleRenderDimension(renderHeight, safeScale);
        }

        const bool shouldLogWindowMetrics = (mLastLoggedDpi != windowDpi)
            || (mLastLoggedDpiScale != windowDpiScale)
            || (mLastLoggedLogicalWidth != windowLogicalWidth)
            || (mLastLoggedLogicalHeight != windowLogicalHeight)
            || (mLastLoggedPhysicalWidth != windowWidth)
            || (mLastLoggedPhysicalHeight != windowHeight)
            || (mLastLoggedSwapchainWidth != windowWidth)
            || (mLastLoggedSwapchainHeight != windowHeight)
            || (mLastLoggedInternalWidth != renderWidth)
            || (mLastLoggedInternalHeight != renderHeight);
        if (shouldLogWindowMetrics) {
            LogInfo(
                TEXT(
                    "Window metrics: DPI={} scale={} logical={}x{} physical={}x{} swapchain={}x{} internal={}x{} renderScale={}."),
                windowDpi, windowDpiScale, windowLogicalWidth, windowLogicalHeight, windowWidth,
                windowHeight, windowWidth, windowHeight, renderWidth, renderHeight,
                mRenderInternalScale);
            mLastLoggedDpi             = windowDpi;
            mLastLoggedDpiScale        = windowDpiScale;
            mLastLoggedLogicalWidth    = windowLogicalWidth;
            mLastLoggedLogicalHeight   = windowLogicalHeight;
            mLastLoggedPhysicalWidth   = windowWidth;
            mLastLoggedPhysicalHeight  = windowHeight;
            mLastLoggedSwapchainWidth  = windowWidth;
            mLastLoggedSwapchainHeight = windowHeight;
            mLastLoggedInternalWidth   = renderWidth;
            mLastLoggedInternalHeight  = renderHeight;
        }

        const u64 frameIndex = (mHostFrameIndex == 0ULL) ? (mFrameIndex + 1ULL) : mHostFrameIndex;
        mFrameIndex          = frameIndex;
        auto       device    = mRhiDevice;
        auto       viewport  = mMainViewport;
        auto       callback  = mRenderCallback;
        auto*      debugGui  = mDebugGui.Get();
        const auto rendererType = Rendering::GetRendererTypeSetting();
        if (rendererType == Rendering::ERendererType::Deferred) {
            mMaterialCache.SetDefaultTemplate(
                Rendering::FBasicDeferredRenderer::GetDefaultMaterialTemplate());
        }

        Engine::FRenderScene                   renderScene;
        TVector<RenderCore::Render::FDrawList> drawLists;
        TVector<RenderCore::Render::FDrawList> shadowDrawLists;

        if (renderWidth > 0U && renderHeight > 0U) {
            if (auto* world = mEngineRuntime.GetWorldManager().GetActiveWorld()) {
                Engine::FSceneViewBuildParams viewParams{};
                viewParams.ViewRect =
                    RenderCore::View::FViewRect{ 0, 0, renderWidth, renderHeight };
                viewParams.RenderTargetExtent =
                    RenderCore::View::FRenderTargetExtent2D{ renderWidth, renderHeight };
                viewParams.FrameIndex          = frameIndex;
                viewParams.DeltaTimeSeconds    = mLastDeltaTimeSeconds;
                viewParams.ViewTarget.Type     = Engine::FSceneView::ETargetType::Viewport;
                viewParams.ViewTarget.Viewport = viewport.Get();
                viewParams.PrimaryCameraOverride =
                    tick.bUseExternalPrimaryCamera ? &tick.ExternalPrimaryCamera : nullptr;

                Engine::FSceneViewBuilder viewBuilder;
                viewBuilder.Build(*world, viewParams, renderScene);

                if (!renderScene.Views.IsEmpty()) {
                    Engine::FSceneBatchBuilder     batchBuilder;
                    Engine::FSceneBatchBuildParams batchParams{};
                    // Instancing stays disabled until the renderer uploads/binds per-instance
                    // transforms for every entry in batch.mInstances. The current base-pass path
                    // only consumes batch.mInstances[0], so enabling batching here would collapse
                    // multiple objects onto the first transform.
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
                        for (const auto& batch : drawList.mBatches) {
                            if (batch.mMaterial != nullptr) {
                                auto* material =
                                    const_cast<RenderCore::FMaterial*>(batch.mMaterial);
                                mMaterialCache.PrepareMaterialForRendering(*material);
                            }
                        }
                    }
                    for (auto& drawList : shadowDrawLists) {
                        for (const auto& batch : drawList.mBatches) {
                            if (batch.mMaterial != nullptr) {
                                auto* material =
                                    const_cast<RenderCore::FMaterial*>(batch.mMaterial);
                                mMaterialCache.PrepareMaterialForRendering(*material);
                            }
                        }
                    }
                }
                DebugAssert(!renderScene.Views.IsEmpty(), TEXT("Launch.EngineLoop"),
                    "Draw: no active scene views generated (width={}, height={}, frame={}).",
                    static_cast<u32>(renderWidth), static_cast<u32>(renderHeight),
                    static_cast<u64>(frameIndex));
            }
        }

        u32 totalBatches = 0U;
        for (const auto& drawList : drawLists) {
            totalBatches += static_cast<u32>(drawList.mBatches.Size());
        }

        if (mDebugGui && mInputSystem) {
            DebugGui::FDebugGuiExternalStats stats{};
            stats.mFrameIndex      = frameIndex;
            stats.mViewCount       = static_cast<u32>(renderScene.Views.Size());
            stats.mSceneBatchCount = totalBatches;
            stats.mDpi             = windowDpi;
            stats.mDpiScale        = windowDpiScale;
            mDebugGui->SetExternalStats(stats);
            mDebugGui->TickGameThread(
                *mInputSystem.Get(), mLastDeltaTimeSeconds, windowWidth, windowHeight);
        }
        // LogInfo(TEXT("Scene Batches: {} (Views: {})"), totalBatches,
        //     static_cast<u32>(renderScene.Views.Size()));

        // LogInfo(TEXT("GameThread Frame {}"), frameIndex);

        auto* assetRegistry = &mAssetRegistry;
        auto* assetManager  = &mAssetManager;

        auto  handle = RenderCore::EnqueueRenderTask(Container::FString(TEXT("RenderFrame")),
             [this, device, viewport, callback, debugGui, frameIndex, windowWidth, windowHeight,
                renderWidth, renderHeight, shouldResize,
                bRedirectPrimaryViewToOffscreen = tick.bRedirectPrimaryViewToOffscreen,
                primaryViewImageId = tick.PrimaryViewImageId, renderScene = Move(renderScene),
                drawLists = Move(drawLists), shadowDrawLists = Move(shadowDrawLists), rendererType,
                assetRegistry, assetManager]() mutable -> void {
                Assert(static_cast<bool>(device), TEXT("Launch.EngineLoop"),
                     "RenderFrame: RHI device is null at frame {}.", static_cast<u64>(frameIndex));
                if (!device)
                    return;

                device->BeginFrame(frameIndex);
                Core::Console::LatchRenderThreadCVars();

                DebugAssert(static_cast<bool>(viewport), TEXT("Launch.EngineLoop"),
                     "RenderFrame: viewport is null at frame {}.", static_cast<u64>(frameIndex));
                if (viewport && windowWidth > 0U && windowHeight > 0U) {
                    if (shouldResize) {
                        viewport->Resize(windowWidth, windowHeight);
                    }

                    Rhi::FRhiTexture* primaryViewOutputOverride = nullptr;
                    if (bRedirectPrimaryViewToOffscreen && renderWidth > 0U && renderHeight > 0U) {
                        Rhi::ERhiFormat targetFormat = Rhi::ERhiFormat::B8G8R8A8Unorm;
                        if (auto* backBuffer = viewport->GetBackBuffer(); backBuffer != nullptr) {
                            targetFormat = backBuffer->GetDesc().mFormat;
                        }

                        auto&      editorOffscreenCache = this->mEditorOffscreenCache;
                        const bool needRecreate         = !editorOffscreenCache.Texture
                            || editorOffscreenCache.Width != renderWidth
                            || editorOffscreenCache.Height != renderHeight
                            || editorOffscreenCache.Format != targetFormat;
                        if (needRecreate) {
                            editorOffscreenCache.Texture.Reset();

                            Rhi::FRhiTextureDesc offscreenDesc{};
                            offscreenDesc.mDebugName.Assign(TEXT("Editor.PrimaryView.Offscreen"));
                            offscreenDesc.mDimension   = Rhi::ERhiTextureDimension::Tex2D;
                            offscreenDesc.mWidth       = renderWidth;
                            offscreenDesc.mHeight      = renderHeight;
                            offscreenDesc.mDepth       = 1U;
                            offscreenDesc.mMipLevels   = 1U;
                            offscreenDesc.mArrayLayers = 1U;
                            offscreenDesc.mFormat      = targetFormat;
                            offscreenDesc.mBindFlags   = Rhi::ERhiTextureBindFlags::RenderTarget
                                | Rhi::ERhiTextureBindFlags::ShaderResource;
                            editorOffscreenCache.Texture = device->CreateTexture(offscreenDesc);
                            editorOffscreenCache.Width   = renderWidth;
                            editorOffscreenCache.Height  = renderHeight;
                            editorOffscreenCache.Format  = targetFormat;
                        }

                        if (editorOffscreenCache.Texture) {
                            primaryViewOutputOverride = editorOffscreenCache.Texture.Get();
                        }
                    }

                    if (debugGui != nullptr && primaryViewImageId != 0ULL) {
                        debugGui->SetImageTexture(primaryViewImageId, primaryViewOutputOverride);
                    }

                    DebugAssert(!renderScene.Views.IsEmpty(), TEXT("Launch.EngineLoop"),
                         "RenderFrame: scene view list is empty at frame {}.",
                         static_cast<u64>(frameIndex));
                    if (!renderScene.Views.IsEmpty()) {
                        // LogInfo(TEXT("Sending Rendering Request {}"), frameIndex);
                        SendSceneRenderingRequest(*device, viewport.Get(), renderScene, drawLists,
                             shadowDrawLists, rendererType, assetRegistry, assetManager,
                             primaryViewOutputOverride);
                    }

                    if (callback) {
                        callback(*device, *viewport, windowWidth, windowHeight);
                    }

                    if (debugGui) {
                        debugGui->RenderRenderThread(*device, *viewport);
                    }

                    const auto queue = device->GetQueue(Rhi::ERhiQueueType::Graphics);
                    DebugAssert(static_cast<bool>(queue), TEXT("Launch.EngineLoop"),
                         "RenderFrame: graphics queue is null at frame {}.",
                         static_cast<u64>(frameIndex));
                    if (queue) {
                        Rhi::FRhiPresentInfo presentInfo{};
                        presentInfo.mViewport     = viewport.Get();
                        presentInfo.mSyncInterval = 1U;
                        queue->Present(presentInfo);
                    }
                } else {
                    DebugAssert(false, TEXT("Launch.EngineLoop"),
                         "RenderFrame: skipped because viewport/extent is invalid (viewport={}, width={}, height={}, frame={}).",
                         static_cast<int>(viewport ? 1 : 0), static_cast<u32>(windowWidth),
                         static_cast<u32>(windowHeight), static_cast<u64>(frameIndex));
                }

                device->EndFrame();

                // LogInfo(TEXT("RenderThread Frame {}"), frameIndex);
            });
        if (handle.IsValid()) {
            mPendingRenderFrames.Push(handle);
            EnforceRenderLag(GetRenderThreadLagFrames());
        }
    }

    void FEngineLoop::Shutdown() {
        mIsRunning = false;

        FlushRenderFrames();
        mEditorOffscreenCache.Texture.Reset();
        mEditorOffscreenCache.Width  = 0U;
        mEditorOffscreenCache.Height = 0U;
        mEditorOffscreenCache.Format = Rhi::ERhiFormat::Unknown;

        if (mDebugGui) {
            mDebugGui.Reset();
        }

        if (mAssetReady) {
            mMaterialCache.Clear();
            mAssetManager.ClearCache();
            mAssetManager.UnregisterLoader(&mTexture2DLoader);
            mAssetManager.UnregisterLoader(&mLevelLoader);
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

        // Release world-owned render resources (meshes/material instances/components) before
        // tearing down the RHI device.
        mEngineRuntime.GetWorldManager().Clear();
        mRenderCallback = {};

        if (mRenderingThread) {
            mRenderingThread->Stop();
            mRenderingThread.Reset();
        }

        ClearSceneRenderCaches();
        Rendering::TemporalAA::ShutdownTemporalAA();
        Rendering::ShutdownPostProcess();
        Rendering::FBasicDeferredRenderer::ShutdownSharedResources();
        Rendering::Atmosphere::FAtmosphereSystem::Get().Reset();
        Rendering::ShutdownMaterialTextureSrvCache();
        RenderCore::ShutdownMaterialFallbacks();

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

#if defined(AE_ENABLE_SCRIPTING_CORECLR) && AE_ENABLE_SCRIPTING_CORECLR
        if (mScriptSystem) {
            mScriptSystem->Shutdown();
            mScriptSystem.Reset();
        }
        Scripting::CoreCLR::SetTransformAccess(
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        Scripting::CoreCLR::SetWindowTitleAccess(nullptr);
        gScriptWorldManager        = nullptr;
        gMainWindowForManagedTitle = nullptr;
#endif

        if (mInputSystem) {
            mInputSystem.Reset();
        }
        mRuntimeInputOverride = nullptr;
    }

    void FEngineLoop::SetRenderCallback(FRenderCallback callback) {
        FlushRenderFrames();
        mRenderCallback = Move(callback);
    }

    void FEngineLoop::SetRuntimeInputOverride(Input::FInputSystem* inputSystem) noexcept {
        mRuntimeInputOverride = inputSystem;
#if defined(AE_ENABLE_SCRIPTING_CORECLR) && AE_ENABLE_SCRIPTING_CORECLR
        if (mScriptSystem) {
            const auto* runtimeInput =
                (mRuntimeInputOverride != nullptr) ? mRuntimeInputOverride : mInputSystem.Get();
            mScriptSystem->SetInputSystem(runtimeInput);
        }
#endif
    }

    auto FEngineLoop::GetServices() noexcept -> FRuntimeServices {
        auto* runtimeInput =
            (mRuntimeInputOverride != nullptr) ? mRuntimeInputOverride : mInputSystem.Get();
        FRuntimeServices services{};
        services.InputSystem    = runtimeInput;
        services.MainWindow     = mApplication ? mApplication->GetMainWindow() : nullptr;
        services.WorldManager   = &mEngineRuntime.GetWorldManager();
        services.AssetRegistry  = &mAssetRegistry;
        services.AssetManager   = &mAssetManager;
        services.DebugGuiSystem = mDebugGui.Get();
        return services;
    }

    auto FEngineLoop::GetServices() const noexcept -> FRuntimeServicesConst {
        const auto* runtimeInput =
            (mRuntimeInputOverride != nullptr) ? mRuntimeInputOverride : mInputSystem.Get();
        FRuntimeServicesConst services{};
        services.InputSystem    = runtimeInput;
        services.MainWindow     = mApplication ? mApplication->GetMainWindow() : nullptr;
        services.WorldManager   = &mEngineRuntime.GetWorldManager();
        services.AssetRegistry  = &mAssetRegistry;
        services.AssetManager   = &mAssetManager;
        services.DebugGuiSystem = mDebugGui.Get();
        return services;
    }

    auto FEngineLoop::GetInputSystem() noexcept -> Input::FInputSystem* {
        return (mRuntimeInputOverride != nullptr) ? mRuntimeInputOverride : mInputSystem.Get();
    }

    auto FEngineLoop::GetInputSystem() const noexcept -> const Input::FInputSystem* {
        return (mRuntimeInputOverride != nullptr) ? mRuntimeInputOverride : mInputSystem.Get();
    }

    auto FEngineLoop::GetPlatformInputSystem() noexcept -> Input::FInputSystem* {
        return mInputSystem.Get();
    }

    auto FEngineLoop::GetPlatformInputSystem() const noexcept -> const Input::FInputSystem* {
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
        Container::FString registryPath;
        const auto&        config            = Core::Utility::EngineConfig::GetGlobalConfig();
        const auto         assetRootOverride = config.GetString(TEXT("GameClient/AssetRoot"));
        LogInfo(TEXT("LoadDemoAssetRegistry AssetRoot override: {}"),
            assetRootOverride.IsEmptyString() ? TEXT("<empty>") : assetRootOverride.ToView());
        if (!assetRootOverride.IsEmptyString()) {
            if (Core::Platform::IsAbsolutePath(assetRootOverride.ToView())) {
                registryPath = assetRootOverride;
            } else {
                const auto exeDir = Core::Platform::GetExecutableDir();
                if (exeDir.IsEmptyString()) {
                    return false;
                }
                registryPath = Core::Utility::Filesystem::FPath(exeDir)
                                   .Append(assetRootOverride.ToView())
                                   .Normalized()
                                   .GetString();
            }
            registryPath.Append(TEXT("/Registry/AssetRegistry.json"));
        } else {
            const auto baseDir = Core::Platform::GetExecutableDir();
            if (baseDir.IsEmptyString()) {
                return false;
            }
            registryPath = baseDir;
            registryPath.Append(TEXT("/Assets/Registry/AssetRegistry.json"));
        }
        LogInfo(TEXT("LoadDemoAssetRegistry path: {}"), registryPath.ToView());
        if (!Core::Platform::IsPathExist(registryPath)) {
            LogWarning(TEXT("Asset registry not found at {}"), registryPath.ToView());
            return false;
        }

        if (!mAssetRegistry.LoadFromJsonFile(registryPath)) {
            LogWarning(TEXT("Failed to load asset registry from {}"), registryPath.ToView());
            return false;
        }

        const auto assetRoot =
            Core::Utility::Filesystem::FPath(registryPath).ParentPath().ParentPath();
        if (!Core::Utility::Filesystem::SetCurrentWorkingDir(assetRoot)) {
            LogWarning(TEXT("Failed to set asset root as working directory."));
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
