#pragma once

#include "RenderCoreAPI.h"

#include "View/CameraData.h"

#include "Math/LinAlg/Common.h"
#include "Math/LinAlg/ProjectionMatrix.h"
#include "Math/LinAlg/SpatialTransform.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::RenderCore::View {
    namespace Math   = Core::Math;
    namespace LinAlg = Core::Math::LinAlg;

    struct AE_RENDER_CORE_API FRenderTargetExtent2D {
        u32 Width  = 0U;
        u32 Height = 0U;

        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool {
            return Width > 0U && Height > 0U;
        }
    };

    struct AE_RENDER_CORE_API FViewRect {
        i32 X      = 0;
        i32 Y      = 0;
        u32 Width  = 0U;
        u32 Height = 0U;

        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool {
            return Width > 0U && Height > 0U;
        }
    };

    struct AE_RENDER_CORE_API FViewMatrixInfo {
        Math::FMatrix4x4f View            = Math::FMatrix4x4f(0.0f);
        Math::FMatrix4x4f ProjUnjittered  = Math::FMatrix4x4f(0.0f);
        Math::FMatrix4x4f ProjJittered    = Math::FMatrix4x4f(0.0f);
        Math::FMatrix4x4f ViewProj        = Math::FMatrix4x4f(0.0f);
        Math::FMatrix4x4f ViewProjJittered = Math::FMatrix4x4f(0.0f);

        Math::FMatrix4x4f InvView         = Math::FMatrix4x4f(0.0f);
        Math::FMatrix4x4f InvProjUnjittered = Math::FMatrix4x4f(0.0f);
        Math::FMatrix4x4f InvProjJittered = Math::FMatrix4x4f(0.0f);
        Math::FMatrix4x4f InvViewProj     = Math::FMatrix4x4f(0.0f);
        Math::FMatrix4x4f InvViewProjJittered = Math::FMatrix4x4f(0.0f);

        Math::FVector2f JitterNdc = Math::FVector2f(0.0f);

        static auto MakeIdentity() noexcept -> FViewMatrixInfo {
            FViewMatrixInfo out{};
            out.View             = LinAlg::Identity<f32, 4>();
            out.ProjUnjittered   = LinAlg::Identity<f32, 4>();
            out.ProjJittered     = LinAlg::Identity<f32, 4>();
            out.ViewProj         = LinAlg::Identity<f32, 4>();
            out.ViewProjJittered = LinAlg::Identity<f32, 4>();

            out.InvView             = LinAlg::Identity<f32, 4>();
            out.InvProjUnjittered   = LinAlg::Identity<f32, 4>();
            out.InvProjJittered     = LinAlg::Identity<f32, 4>();
            out.InvViewProj         = LinAlg::Identity<f32, 4>();
            out.InvViewProjJittered = LinAlg::Identity<f32, 4>();
            out.JitterNdc           = Math::FVector2f(0.0f);
            return out;
        }

        [[nodiscard]] static constexpr auto ApplyJitterToProjection(
            const Math::FMatrix4x4f& proj, const Math::FVector2f& jitterNdc) noexcept
            -> Math::FMatrix4x4f {
            Math::FMatrix4x4f out = proj;
            out(0, 2) += jitterNdc[0];
            out(1, 2) += jitterNdc[1];
            return out;
        }

        [[nodiscard]] static constexpr auto MakeOrthoProj(
            f32 width, f32 height, f32 nearPlane, f32 farPlane) noexcept -> Math::FMatrix4x4f {
            Math::FMatrix4x4f out(0.0f);
            if (width == 0.0f || height == 0.0f) {
                return out;
            }

            const f32 zRange = farPlane - nearPlane;
            out(0, 0)        = 2.0f / width;
            out(1, 1)        = 2.0f / height;
            out(2, 2)        = (zRange != 0.0f) ? (1.0f / zRange) : 0.0f;
            out(2, 3)        = (zRange != 0.0f) ? (-nearPlane / zRange) : 0.0f;
            out(3, 3)        = 1.0f;
            return out;
        }

        [[nodiscard]] static constexpr auto MakeOrthoProjReversedZ(
            f32 width, f32 height, f32 nearPlane, f32 farPlane) noexcept -> Math::FMatrix4x4f {
            Math::FMatrix4x4f out(0.0f);
            if (width == 0.0f || height == 0.0f) {
                return out;
            }

            const f32 zRange = nearPlane - farPlane;
            out(0, 0)        = 2.0f / width;
            out(1, 1)        = 2.0f / height;
            out(2, 2)        = (zRange != 0.0f) ? (1.0f / zRange) : 0.0f;
            out(2, 3)        = (zRange != 0.0f) ? (-farPlane / zRange) : 0.0f;
            out(3, 3)        = 1.0f;
            return out;
        }

        void BuildFromCamera(const FCameraData& camera, const FViewRect& viewRect,
            const Math::FVector2f& jitterNdc, bool bReverseZ) noexcept {
            JitterNdc = jitterNdc;

            const Math::FMatrix4x4f cameraWorld = camera.Transform.ToMatrix();
            View                               = LinAlg::Inverse(cameraWorld);
            InvView                            = cameraWorld;

            const f32 viewX = static_cast<f32>(viewRect.Width);
            const f32 viewY = static_cast<f32>(viewRect.Height);

            if (camera.ProjectionType == ECameraProjectionType::Perspective) {
                if (bReverseZ) {
                    ProjUnjittered = LinAlg::FReversedZProjectionMatrixf(
                        camera.VerticalFovRadians, viewX, viewY, camera.NearPlane, camera.FarPlane);
                } else {
                    ProjUnjittered = LinAlg::FProjectionMatrixf(
                        camera.VerticalFovRadians, viewX, viewY, camera.NearPlane, camera.FarPlane);
                }
            } else {
                ProjUnjittered = bReverseZ
                    ? MakeOrthoProjReversedZ(camera.OrthoWidth, camera.OrthoHeight, camera.NearPlane,
                          camera.FarPlane)
                    : MakeOrthoProj(camera.OrthoWidth, camera.OrthoHeight, camera.NearPlane,
                          camera.FarPlane);
            }

            ProjJittered = ApplyJitterToProjection(ProjUnjittered, jitterNdc);

            ViewProj         = Math::MatMul(ProjUnjittered, View);
            ViewProjJittered = Math::MatMul(ProjJittered, View);

            InvProjUnjittered   = LinAlg::Inverse(ProjUnjittered);
            InvProjJittered     = LinAlg::Inverse(ProjJittered);
            InvViewProj         = LinAlg::Inverse(ViewProj);
            InvViewProjJittered = LinAlg::Inverse(ViewProjJittered);
        }
    };

    struct AE_RENDER_CORE_API FPreviousViewData {
        bool bHasValidHistory = false;
        bool bCameraCut       = false;

        u64 FrameIndex          = 0ULL;
        u32 TemporalSampleIndex = 0U;
        f32 DeltaTimeSeconds    = 0.0f;

        Math::FVector3f ViewOrigin = Math::FVector3f(0.0f);
        FViewMatrixInfo Matrices   = FViewMatrixInfo::MakeIdentity();

        void Invalidate() noexcept {
            bHasValidHistory = false;
            bCameraCut       = true;
        }
    };

    struct AE_RENDER_CORE_API FViewData {
        FCameraData Camera;

        FViewRect             ViewRect;
        FRenderTargetExtent2D RenderTargetExtent;

        bool bReverseZ = true;

        u64 FrameIndex          = 0ULL;
        u32 TemporalSampleIndex = 0U;
        f32 DeltaTimeSeconds    = 0.0f;

        Math::FVector3f ViewOrigin = Math::FVector3f(0.0f);
        FViewMatrixInfo Matrices   = FViewMatrixInfo::MakeIdentity();

        FPreviousViewData Previous;

        void UpdateMatrices(const Math::FVector2f& jitterNdc = Math::FVector2f(0.0f)) noexcept {
            ViewOrigin = Camera.GetPosition();
            Matrices.BuildFromCamera(Camera, ViewRect, jitterNdc, bReverseZ);
        }

        void BeginFrame(const Math::FVector2f& jitterNdc = Math::FVector2f(0.0f)) noexcept {
            if (Camera.bCameraCut) {
                Previous.Invalidate();
            }

            UpdateMatrices(jitterNdc);
        }

        void EndFrame() noexcept {
            Previous.bHasValidHistory   = true;
            Previous.bCameraCut         = Camera.bCameraCut;
            Previous.FrameIndex         = FrameIndex;
            Previous.TemporalSampleIndex = TemporalSampleIndex;
            Previous.DeltaTimeSeconds   = DeltaTimeSeconds;
            Previous.ViewOrigin         = ViewOrigin;
            Previous.Matrices           = Matrices;
        }

        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool {
            return ViewRect.IsValid() && RenderTargetExtent.IsValid();
        }
    };

} // namespace AltinaEngine::RenderCore::View
