#include "Rendering/TemporalAA/TemporalAA.h"

#include "TemporalAA/TemporalAAState.h"

#include "Logging/Log.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiTexture.h"
#include "View/TemporalJitter.h"

namespace AltinaEngine::Rendering::TemporalAA::Detail {
    namespace {
        using Container::THashMap;

        auto GetStateMap() -> THashMap<u64, FTemporalAAViewState>& {
            static THashMap<u64, FTemporalAAViewState> s{};
            return s;
        }

        auto CreateHistoryTexture(Rhi::FRhiDevice& device, const TChar* debugName, u32 width,
            u32 height, Rhi::ERhiFormat format) -> Rhi::FRhiTextureRef {
            Rhi::FRhiTextureDesc desc{};
            desc.mDebugName.Assign(debugName);
            desc.mWidth       = width;
            desc.mHeight      = height;
            desc.mArrayLayers = 1U;
            desc.mMipLevels   = 1U;
            desc.mFormat      = format;
            desc.mBindFlags =
                Rhi::ERhiTextureBindFlags::RenderTarget | Rhi::ERhiTextureBindFlags::ShaderResource;
            return device.CreateTexture(desc);
        }

        [[nodiscard]] auto HistoryRead(const FTemporalAAViewState& s) -> Rhi::FRhiTextureRef {
            return s.bHistoryToggle ? s.HistoryB : s.HistoryA;
        }

        [[nodiscard]] auto HistoryWrite(const FTemporalAAViewState& s) -> Rhi::FRhiTextureRef {
            return s.bHistoryToggle ? s.HistoryA : s.HistoryB;
        }
    } // namespace

    auto GetOrCreateViewState(u64 viewKey) -> FTemporalAAViewState& {
        auto& map = GetStateMap();
        if (!map.HasKey(viewKey)) {
            map[viewKey] = FTemporalAAViewState{};
        }
        return map[viewKey];
    }

    void EnsureHistoryTextures(
        Rhi::FRhiDevice& device, u64 viewKey, u32 width, u32 height, Rhi::ERhiFormat format) {
        auto&      s = GetOrCreateViewState(viewKey);

        const bool bNeedRealloc = (s.Width != width) || (s.Height != height)
            || (s.HistoryFormat != format) || !s.HistoryA || !s.HistoryB;

        if (!bNeedRealloc) {
            return;
        }

        s.Width          = width;
        s.Height         = height;
        s.HistoryFormat  = format;
        s.bHistoryToggle = false;
        s.HistoryA.Reset();
        s.HistoryB.Reset();
        s.bValidHistory = false;

        if (width == 0U || height == 0U || format == Rhi::ERhiFormat::Unknown) {
            s.InvalidateHistory();
            return;
        }

        s.HistoryA =
            CreateHistoryTexture(device, TEXT("TemporalAA.HistoryA"), width, height, format);
        s.HistoryB =
            CreateHistoryTexture(device, TEXT("TemporalAA.HistoryB"), width, height, format);
        if (!s.HistoryA || !s.HistoryB) {
            LogErrorCat(TEXT("Rendering.TAA"),
                TEXT("TemporalAA: failed to allocate history textures ({}x{}, format={})."), width,
                height, static_cast<u32>(format));
            s.InvalidateHistory();
            return;
        }

        // History content is undefined until the first successful write.
        s.InvalidateHistory();
    }

    void CommitHistoryWritten(u64 viewKey) noexcept {
        auto& s = GetOrCreateViewState(viewKey);
        if (!s.HistoryA || !s.HistoryB) {
            return;
        }

        // After the TAA pass writes HistoryWrite, flip so the written texture becomes the read
        // texture for the next frame.
        s.bValidHistory  = true;
        s.bHistoryToggle = !s.bHistoryToggle;
    }

    [[nodiscard]] auto GetHistoryReadTexture(u64 viewKey) -> Rhi::FRhiTextureRef {
        const auto& s = GetOrCreateViewState(viewKey);
        return HistoryRead(s);
    }

    [[nodiscard]] auto GetHistoryWriteTexture(u64 viewKey) -> Rhi::FRhiTextureRef {
        const auto& s = GetOrCreateViewState(viewKey);
        return HistoryWrite(s);
    }

    void ClearAllViewStates() noexcept {
        auto& map = GetStateMap();
        map.Clear();
    }
} // namespace AltinaEngine::Rendering::TemporalAA::Detail

namespace AltinaEngine::Rendering::TemporalAA {
    void PrepareViewForFrame(
        u64 viewKey, RenderCore::View::FViewData& view, bool bEnableJitter, u32 sampleCount) {
        auto&     state = Detail::GetOrCreateViewState(viewKey);

        // Resize / invalidation.
        const u32 w = view.RenderTargetExtent.Width;
        const u32 h = view.RenderTargetExtent.Height;
        if (w == 0U || h == 0U) {
            state.InvalidateHistory();
            state.ResetTemporalIndex();
        } else if (state.Width != 0U && state.Height != 0U
            && (state.Width != w || state.Height != h)) {
            state.InvalidateHistory();
            state.ResetTemporalIndex();
        }

        // Persisted previous frame snapshot.
        view.Previous = state.PrevView;

        // Propagate history validity to the view (used by TAA to bypass history).
        view.Previous.bHasValidHistory = state.bValidHistory && view.Previous.bHasValidHistory;

        // Temporal jitter.
        view.TemporalSampleIndex = state.TemporalSampleIndex;
        const auto jitterNdc     = bEnableJitter
                ? RenderCore::View::ComputeHalton23JitterNdc(
                  state.TemporalSampleIndex, sampleCount, view.ViewRect.Width, view.ViewRect.Height)
                : Core::Math::FVector2f(0.0f);

        view.BeginFrame(jitterNdc);
    }

    void FinalizeViewForFrame(
        u64 viewKey, const RenderCore::View::FViewData& view, bool bEnableJitter, u32 sampleCount) {
        auto& state = Detail::GetOrCreateViewState(viewKey);

        // Camera cut invalidates history + resets jitter.
        if (view.Camera.mCameraCut) {
            state.InvalidateHistory();
            state.ResetTemporalIndex();
        }

        // Persist current view snapshot (equivalent to FViewData::EndFrame(), but cached across
        // frames).
        state.PrevView.bHasValidHistory    = state.bValidHistory;
        state.PrevView.bCameraCut          = view.Camera.mCameraCut;
        state.PrevView.FrameIndex          = view.FrameIndex;
        state.PrevView.TemporalSampleIndex = view.TemporalSampleIndex;
        state.PrevView.DeltaTimeSeconds    = view.DeltaTimeSeconds;
        state.PrevView.ViewOrigin          = view.ViewOrigin;
        state.PrevView.Matrices            = view.Matrices;

        // Advance for next frame.
        if (bEnableJitter) {
            if (sampleCount == 0U) {
                sampleCount = 1U;
            }
            state.TemporalSampleIndex = (state.TemporalSampleIndex + 1U) % sampleCount;
        } else {
            state.TemporalSampleIndex = 0U;
        }
    }

    void ShutdownTemporalAA() noexcept { Detail::ClearAllViewStates(); }
} // namespace AltinaEngine::Rendering::TemporalAA
