#include "Shadow/CascadedShadowMapping.h"

#include "Math/LinAlg/Common.h"
#include "Math/LinAlg/LookAt.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace AltinaEngine::RenderCore::Shadow {
    namespace {
        using Math::FMatrix4x4f;
        using Math::FVector2f;
        using Math::FVector3f;
        using Math::FVector4f;
        namespace LinAlg = Core::Math::LinAlg;

        constexpr f32      kEps = 1e-4f;

        [[nodiscard]] auto ClampCascadeCount(u32 count) noexcept -> u32 {
            if (count == 0U) {
                return 0U;
            }
            return (count > kMaxCascades) ? kMaxCascades : count;
        }

        [[nodiscard]] auto SafeNormalize(const FVector3f& v) noexcept -> FVector3f {
            const f32 x    = v.mComponents[0];
            const f32 y    = v.mComponents[1];
            const f32 z    = v.mComponents[2];
            const f32 len2 = x * x + y * y + z * z;
            if (len2 <= kEps) {
                return FVector3f(0.0f, -1.0f, 0.0f);
            }
            const f32 invLen = 1.0f / std::sqrt(len2);
            return FVector3f(x * invLen, y * invLen, z * invLen);
        }

        [[nodiscard]] auto Lerp(f32 a, f32 b, f32 t) noexcept -> f32 { return a + (b - a) * t; }

        void               BuildCascadeSplits(
                          f32 nearVS, f32 farVS, u32 cascadeCount, f32 lambda, FCSMData& outCsm) {
            cascadeCount        = ClampCascadeCount(cascadeCount);
            outCsm.CascadeCount = cascadeCount;
            if (cascadeCount == 0U) {
                return;
            }

            const f32 nearD = std::max(nearVS, kEps);
            const f32 farD  = std::max(farVS, nearD + kEps);
            const f32 ratio = farD / nearD;

            f32       prev = nearD;
            for (u32 i = 1U; i <= cascadeCount; ++i) {
                const f32 si    = static_cast<f32>(i) / static_cast<f32>(cascadeCount);
                const f32 logD  = nearD * std::pow(ratio, si);
                const f32 uniD  = nearD + (farD - nearD) * si;
                const f32 split = Lerp(uniD, logD, std::clamp(lambda, 0.0f, 1.0f));

                const u32 idx                = i - 1U;
                outCsm.Cascades[idx].SplitVS = FVector2f(prev, split);
                prev                         = split;
            }
        }

        void BuildFrustumCornersVS(
            f32 nearVS, f32 farVS, f32 aspect, f32 tanHalfFovY, FVector3f outCorners[8]) {
            const f32 nearH = nearVS * tanHalfFovY;
            const f32 nearW = nearH * aspect;
            const f32 farH  = farVS * tanHalfFovY;
            const f32 farW  = farH * aspect;

            // Near plane (z = nearVS)
            outCorners[0] = FVector3f(-nearW, -nearH, nearVS);
            outCorners[1] = FVector3f(nearW, -nearH, nearVS);
            outCorners[2] = FVector3f(nearW, nearH, nearVS);
            outCorners[3] = FVector3f(-nearW, nearH, nearVS);

            // Far plane (z = farVS)
            outCorners[4] = FVector3f(-farW, -farH, farVS);
            outCorners[5] = FVector3f(farW, -farH, farVS);
            outCorners[6] = FVector3f(farW, farH, farVS);
            outCorners[7] = FVector3f(-farW, farH, farVS);
        }
    } // namespace

    void BuildDirectionalCSM(const View::FViewData& view, const Lighting::FDirectionalLight& light,
        const FCSMSettings& settings, FCSMData& outCsm) {
        outCsm = {};

        if (!view.IsValid()) {
            return;
        }

        const u32 cascadeCount = ClampCascadeCount(settings.CascadeCount);
        if (cascadeCount == 0U) {
            return;
        }

        // View-space distances (positive Z forward for the engine's LH camera convention).
        const f32 nearVS = std::max(view.Camera.NearPlane, kEps);
        const f32 farVS =
            std::max(std::min(view.Camera.FarPlane, settings.MaxDistance), nearVS + kEps);

        BuildCascadeSplits(nearVS, farVS, cascadeCount, settings.SplitLambda, outCsm);

        const f32 aspect      = (view.ViewRect.Height > 0U)
                 ? (static_cast<f32>(view.ViewRect.Width) / static_cast<f32>(view.ViewRect.Height))
                 : 1.0f;
        const f32 tanHalfFovY = std::tan(std::max(view.Camera.VerticalFovRadians * 0.5f, 0.001f));

        const FVector3f dirWS = SafeNormalize(light.DirectionWS);

        for (u32 cascadeIndex = 0U; cascadeIndex < cascadeCount; ++cascadeIndex) {
            const f32 splitNear = outCsm.Cascades[cascadeIndex].SplitVS[0];
            const f32 splitFar  = outCsm.Cascades[cascadeIndex].SplitVS[1];

            FVector3f cornersVS[8] = {
                FVector3f(0.0f, 0.0f, 0.0f),
                FVector3f(0.0f, 0.0f, 0.0f),
                FVector3f(0.0f, 0.0f, 0.0f),
                FVector3f(0.0f, 0.0f, 0.0f),
                FVector3f(0.0f, 0.0f, 0.0f),
                FVector3f(0.0f, 0.0f, 0.0f),
                FVector3f(0.0f, 0.0f, 0.0f),
                FVector3f(0.0f, 0.0f, 0.0f),
            };
            BuildFrustumCornersVS(splitNear, splitFar, aspect, tanHalfFovY, cornersVS);

            // Transform to world space using InvView.
            FVector3f cornersWS[8] = {
                FVector3f(0.0f, 0.0f, 0.0f),
                FVector3f(0.0f, 0.0f, 0.0f),
                FVector3f(0.0f, 0.0f, 0.0f),
                FVector3f(0.0f, 0.0f, 0.0f),
                FVector3f(0.0f, 0.0f, 0.0f),
                FVector3f(0.0f, 0.0f, 0.0f),
                FVector3f(0.0f, 0.0f, 0.0f),
                FVector3f(0.0f, 0.0f, 0.0f),
            };
            FVector3f centerWS(0.0f, 0.0f, 0.0f);
            for (u32 i = 0U; i < 8U; ++i) {
                const auto p4   = Math::MatMul(view.Matrices.InvView,
                      FVector4f(cornersVS[i][0], cornersVS[i][1], cornersVS[i][2], 1.0f));
                const f32  invW = (std::abs(p4[3]) > kEps) ? (1.0f / p4[3]) : 1.0f;
                cornersWS[i]    = FVector3f(p4[0] * invW, p4[1] * invW, p4[2] * invW);
                centerWS[0] += cornersWS[i][0];
                centerWS[1] += cornersWS[i][1];
                centerWS[2] += cornersWS[i][2];
            }
            centerWS[0] *= (1.0f / 8.0f);
            centerWS[1] *= (1.0f / 8.0f);
            centerWS[2] *= (1.0f / 8.0f);

            // Build a light view matrix looking at the cascade center.
            const FVector3f upCandidate(0.0f, 1.0f, 0.0f);
            // If dir is close to up, pick a different up.
            const f32       upDot =
                dirWS[0] * upCandidate[0] + dirWS[1] * upCandidate[1] + dirWS[2] * upCandidate[2];
            const FVector3f upWS =
                (std::abs(upDot) > 0.98f) ? FVector3f(0.0f, 0.0f, 1.0f) : upCandidate;

            const f32         lightDistance = splitFar; // good-enough anchor distance
            const FVector3f   eyeWS(centerWS[0] - dirWS[0] * lightDistance,
                  centerWS[1] - dirWS[1] * lightDistance, centerWS[2] - dirWS[2] * lightDistance);

            const FMatrix4x4f lightView = LinAlg::LookAtLH(eyeWS, centerWS, upWS);

            // Find light-space AABB of the cascade frustum.
            FVector3f         minLS(FLT_MAX, FLT_MAX, FLT_MAX);
            FVector3f         maxLS(-FLT_MAX, -FLT_MAX, -FLT_MAX);

            for (u32 i = 0U; i < 8U; ++i) {
                const auto p4 = Math::MatMul(
                    lightView, FVector4f(cornersWS[i][0], cornersWS[i][1], cornersWS[i][2], 1.0f));
                minLS[0] = std::min(minLS[0], p4[0]);
                minLS[1] = std::min(minLS[1], p4[1]);
                minLS[2] = std::min(minLS[2], p4[2]);
                maxLS[0] = std::max(maxLS[0], p4[0]);
                maxLS[1] = std::max(maxLS[1], p4[1]);
                maxLS[2] = std::max(maxLS[2], p4[2]);
            }

            // Expand a little to reduce edge artifacts.
            const f32 zPad = 50.0f;
            minLS[2] -= zPad;
            maxLS[2] += zPad;

            const f32       width  = std::max(maxLS[0] - minLS[0], kEps);
            const f32       height = std::max(maxLS[1] - minLS[1], kEps);
            const f32       nearZ  = std::min(minLS[2], maxLS[2]);
            const f32       farZ   = std::max(minLS[2], maxLS[2]);

            // Ortho projection centered on the AABB.
            const FVector3f boxCenter(
                (minLS[0] + maxLS[0]) * 0.5f, (minLS[1] + maxLS[1]) * 0.5f, 0.0f);
            // Translate so the AABB is centered.
            FMatrix4x4f translate = LinAlg::Identity<f32, 4>();
            translate(0, 3)       = -boxCenter[0];
            translate(1, 3)       = -boxCenter[1];

            const FMatrix4x4f lightViewCentered = Math::MatMul(translate, lightView);
            const FMatrix4x4f lightProj =
                View::FViewMatrixInfo::MakeOrthoProjReversedZ(width, height, nearZ, farZ);

            outCsm.Cascades[cascadeIndex].LightViewProj =
                Math::MatMul(lightProj, lightViewCentered);
        }
    }
} // namespace AltinaEngine::RenderCore::Shadow
