#pragma once

#include "Math/Matrix.h"
#include "Shadow/CascadedShadowMapping.h"

namespace AltinaEngine::Rendering::Deferred {
    namespace Math = Core::Math;
    using Math::FMatrix4x4f;

    inline constexpr u32 kMaxPointLights = 16U;

    struct FPerFrameConstants {
        FMatrix4x4f ViewProjection;

        FMatrix4x4f View;
        FMatrix4x4f Proj;
        FMatrix4x4f ViewProj;
        FMatrix4x4f InvViewProj;

        f32         ViewOriginWS[3] = { 0.0f, 0.0f, 0.0f };
        u32         PointLightCount = 0U;

        f32         DirLightDirectionWS[3] = { 0.0f, -1.0f, 0.0f };
        f32         DirLightIntensity      = 0.0f;
        f32         DirLightColor[3]       = { 1.0f, 1.0f, 1.0f };
        u32         CSMCascadeCount        = 0U;

        FMatrix4x4f CSM_LightViewProj0{};
        FMatrix4x4f CSM_LightViewProj1{};
        FMatrix4x4f CSM_LightViewProj2{};
        FMatrix4x4f CSM_LightViewProj3{};
        f32         CSM_SplitsVS[RenderCore::Shadow::kMaxCascades][4] = {};

        f32         RenderTargetSize[2]    = { 0.0f, 0.0f };
        f32         InvRenderTargetSize[2] = { 0.0f, 0.0f };

        u32         bReverseZ           = 1U;
        u32         DebugShadingMode    = 0U;
        f32         ShadowBias          = 0.0015f;
        f32         _pad0               = 0.0f;
        f32         ShadowMapInvSize[2] = { 0.0f, 0.0f };
        f32         _pad1[2]            = { 0.0f, 0.0f };

        struct FPointLight {
            f32 PositionWS[3] = { 0.0f, 0.0f, 0.0f };
            f32 Range         = 0.0f;
            f32 Color[3]      = { 1.0f, 1.0f, 1.0f };
            f32 Intensity     = 0.0f;
        };
        FPointLight PointLights[kMaxPointLights]{};
    };

    struct FPerDrawConstants {
        FMatrix4x4f World;
        FMatrix4x4f NormalMatrix;
    };

    struct FIblConstants {
        f32 EnvDiffuseIntensity  = 0.0f;
        f32 EnvSpecularIntensity = 0.0f;
        f32 SpecularMaxLod       = 0.0f;
        f32 EnvSaturation        = 1.0f;
    };
} // namespace AltinaEngine::Rendering::Deferred
