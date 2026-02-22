#pragma once

#include "RenderCoreAPI.h"

#include "Container/Vector.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiStructs.h"

using AltinaEngine::Forward;
using AltinaEngine::TDecay;
namespace AltinaEngine::RenderCore {
    namespace Container = Core::Container;
    using Container::TVector;

    enum class EFrameGraphQueue : u8 {
        Graphics = 0,
        Compute,
        Copy
    };

    enum class EFrameGraphPassType : u8 {
        Raster = 0,
        Compute,
        Copy
    };

    enum class EFrameGraphPassFlags : u8 {
        None           = 0,
        NeverCull      = 1u << 0,
        ExternalOutput = 1u << 1
    };

    [[nodiscard]] constexpr auto operator|(
        EFrameGraphPassFlags lhs, EFrameGraphPassFlags rhs) noexcept -> EFrameGraphPassFlags {
        return static_cast<EFrameGraphPassFlags>(ToUnderlying(lhs) | ToUnderlying(rhs));
    }

    [[nodiscard]] constexpr auto operator&(
        EFrameGraphPassFlags lhs, EFrameGraphPassFlags rhs) noexcept -> EFrameGraphPassFlags {
        return static_cast<EFrameGraphPassFlags>(ToUnderlying(lhs) & ToUnderlying(rhs));
    }

    constexpr auto operator|=(EFrameGraphPassFlags& lhs, EFrameGraphPassFlags rhs) noexcept
        -> EFrameGraphPassFlags& {
        lhs = lhs | rhs;
        return lhs;
    }

    [[nodiscard]] constexpr auto HasAnyFlags(
        EFrameGraphPassFlags value, EFrameGraphPassFlags flags) noexcept -> bool {
        return (ToUnderlying(value) & ToUnderlying(flags)) != 0;
    }

    struct FFrameGraphTextureDesc {
        Rhi::FRhiTextureDesc   mDesc;
        Rhi::ERhiResourceState mInitialState = Rhi::ERhiResourceState::Common;
    };

    struct FFrameGraphBufferDesc {
        Rhi::FRhiBufferDesc    mDesc;
        Rhi::ERhiResourceState mInitialState = Rhi::ERhiResourceState::Common;
    };

    struct FFrameGraphTextureRef {
        u32                          mId = 0;
        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool { return mId != 0U; }
    };

    struct FFrameGraphBufferRef {
        u32                          mId = 0;
        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool { return mId != 0U; }
    };

    struct FFrameGraphSRVRef {
        u32                          mId = 0;
        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool { return mId != 0U; }
    };

    struct FFrameGraphUAVRef {
        u32                          mId = 0;
        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool { return mId != 0U; }
    };

    struct FFrameGraphRTVRef {
        u32                          mId = 0;
        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool { return mId != 0U; }
    };

    struct FFrameGraphDSVRef {
        u32                          mId = 0;
        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool { return mId != 0U; }
    };

    class FFrameGraph;

    class AE_RENDER_CORE_API FFrameGraphPassResources {
    public:
        [[nodiscard]] auto GetTexture(FFrameGraphTextureRef ref) const -> Rhi::FRhiTexture*;
        [[nodiscard]] auto GetBuffer(FFrameGraphBufferRef ref) const -> Rhi::FRhiBuffer*;
        [[nodiscard]] auto GetSRV(FFrameGraphSRVRef ref) const -> Rhi::FRhiShaderResourceView*;
        [[nodiscard]] auto GetUAV(FFrameGraphUAVRef ref) const -> Rhi::FRhiUnorderedAccessView*;
        [[nodiscard]] auto GetRTV(FFrameGraphRTVRef ref) const -> Rhi::FRhiRenderTargetView*;
        [[nodiscard]] auto GetDSV(FFrameGraphDSVRef ref) const -> Rhi::FRhiDepthStencilView*;

    private:
        friend class FFrameGraph;
        explicit FFrameGraphPassResources(const FFrameGraph& graph) : mGraph(&graph) {}

        const FFrameGraph* mGraph = nullptr;
    };

    struct FRdgRenderTargetBinding {
        FFrameGraphRTVRef   mRTV;
        Rhi::ERhiLoadOp     mLoadOp     = Rhi::ERhiLoadOp::Load;
        Rhi::ERhiStoreOp    mStoreOp    = Rhi::ERhiStoreOp::Store;
        Rhi::FRhiClearColor mClearColor = {};
    };

    struct FRdgDepthStencilBinding {
        FFrameGraphDSVRef          mDSV;
        Rhi::ERhiLoadOp            mDepthLoadOp       = Rhi::ERhiLoadOp::Load;
        Rhi::ERhiStoreOp           mDepthStoreOp      = Rhi::ERhiStoreOp::Store;
        Rhi::ERhiLoadOp            mStencilLoadOp     = Rhi::ERhiLoadOp::Load;
        Rhi::ERhiStoreOp           mStencilStoreOp    = Rhi::ERhiStoreOp::Store;
        Rhi::FRhiClearDepthStencil mClearDepthStencil = {};
    };

    class AE_RENDER_CORE_API FFrameGraphPassBuilder {
    public:
        FFrameGraphTextureRef CreateTexture(const FFrameGraphTextureDesc& desc);
        FFrameGraphBufferRef  CreateBuffer(const FFrameGraphBufferDesc& desc);

        FFrameGraphTextureRef Read(FFrameGraphTextureRef tex, Rhi::ERhiResourceState state);
        FFrameGraphTextureRef Write(FFrameGraphTextureRef tex, Rhi::ERhiResourceState state);
        FFrameGraphBufferRef  Read(FFrameGraphBufferRef buf, Rhi::ERhiResourceState state);
        FFrameGraphBufferRef  Write(FFrameGraphBufferRef buf, Rhi::ERhiResourceState state);

        FFrameGraphTextureRef Read(FFrameGraphTextureRef tex, Rhi::ERhiResourceState state,
            const Rhi::FRhiTextureViewRange& range);
        FFrameGraphTextureRef Write(FFrameGraphTextureRef tex, Rhi::ERhiResourceState state,
            const Rhi::FRhiTextureViewRange& range);

        FFrameGraphSRVRef     CreateSRV(
                FFrameGraphTextureRef tex, const Rhi::FRhiShaderResourceViewDesc& desc);
        FFrameGraphUAVRef CreateUAV(
            FFrameGraphTextureRef tex, const Rhi::FRhiUnorderedAccessViewDesc& desc);
        FFrameGraphSRVRef CreateSRV(
            FFrameGraphBufferRef buf, const Rhi::FRhiShaderResourceViewDesc& desc);
        FFrameGraphUAVRef CreateUAV(
            FFrameGraphBufferRef buf, const Rhi::FRhiUnorderedAccessViewDesc& desc);
        FFrameGraphRTVRef CreateRTV(
            FFrameGraphTextureRef tex, const Rhi::FRhiRenderTargetViewDesc& desc);
        FFrameGraphDSVRef CreateDSV(
            FFrameGraphTextureRef tex, const Rhi::FRhiDepthStencilViewDesc& desc);

        void SetRenderTargets(
            const FRdgRenderTargetBinding* RTVs, u32 RTVCount, const FRdgDepthStencilBinding* DSV);
        void SetExternalOutput(FFrameGraphTextureRef tex, Rhi::ERhiResourceState finalState);
        void SetSideEffect();

    private:
        friend class FFrameGraph;
        FFrameGraphPassBuilder(FFrameGraph& graph, u32 passIndex)
            : mGraph(graph), mPassIndex(passIndex) {}

        FFrameGraph& mGraph;
        u32          mPassIndex = 0;
    };

    using TRdgPassExecuteFn = void (*)(
        Rhi::FRhiCmdContext& ctx, const FFrameGraphPassResources& res);

    template <typename PassData>
    using TRdgPassExecuteWithDataFn = void (*)(
        Rhi::FRhiCmdContext& ctx, const FFrameGraphPassResources& res, const PassData& data);

    struct FFrameGraphPassDesc {
        const char*          mName    = "UnnamedPass";
        EFrameGraphPassType  mType    = EFrameGraphPassType::Raster;
        EFrameGraphQueue     mQueue   = EFrameGraphQueue::Graphics;
        EFrameGraphPassFlags mFlags   = EFrameGraphPassFlags::None;
        TRdgPassExecuteFn    mExecute = nullptr;
    };

    class AE_RENDER_CORE_API FFrameGraph {
    public:
        explicit FFrameGraph(Rhi::FRhiDevice& device);
        ~FFrameGraph();

        FFrameGraph(const FFrameGraph&)                    = delete;
        auto operator=(const FFrameGraph&) -> FFrameGraph& = delete;
        FFrameGraph(FFrameGraph&&)                         = delete;
        auto operator=(FFrameGraph&&) -> FFrameGraph&      = delete;

        void BeginFrame(u64 frameIndex);
        void EndFrame();

        template <typename PassData, typename SetupFunc, typename ExecuteFunc>
        void AddPass(const FFrameGraphPassDesc& desc, SetupFunc&& setup, ExecuteFunc&& execute) {
            const u32 passIndex = AllocatePass(desc);
            auto&     pass      = mPasses[passIndex];

            using TPassData       = PassData;
            TPassData* data       = new TPassData();
            pass.mPassData        = data;
            pass.mDestroyPassData = &DestroyPassData<TPassData>;

            using TExecute        = typename TDecay<ExecuteFunc>::TType;
            TExecute* exec        = new TExecute(Forward<ExecuteFunc>(execute));
            pass.mExecuteUserData = exec;
            pass.mExecute         = &ExecuteWithData<TPassData, TExecute>;
            pass.mDestroyExecute  = &DestroyExecute<TExecute>;

            FFrameGraphPassBuilder builder(*this, passIndex);
            if constexpr (requires { setup(builder, *data); }) {
                Forward<SetupFunc>(setup)(builder, *data);
            } else if constexpr (requires { setup(builder); }) {
                Forward<SetupFunc>(setup)(builder);
            } else {
                static_assert(sizeof(SetupFunc) == 0,
                    "SetupFunc must accept (FFrameGraphPassBuilder&) or (FFrameGraphPassBuilder&, PassData&)");
            }
        }

        template <typename SetupFunc>
        void AddPass(const FFrameGraphPassDesc& desc, SetupFunc&& setup) {
            const u32              passIndex = AllocatePass(desc);
            FFrameGraphPassBuilder builder(*this, passIndex);
            if constexpr (requires { setup(builder); }) {
                Forward<SetupFunc>(setup)(builder);
            } else {
                static_assert(
                    sizeof(SetupFunc) == 0, "SetupFunc must accept (FFrameGraphPassBuilder&)");
            }
        }

        void                  Compile();
        void                  Execute(Rhi::FRhiCmdContext& cmdContext);

        FFrameGraphTextureRef ImportTexture(
            Rhi::FRhiTexture* external, Rhi::ERhiResourceState state);
        FFrameGraphBufferRef ImportBuffer(Rhi::FRhiBuffer* external, Rhi::ERhiResourceState state);

    private:
        friend class FFrameGraphPassResources;
        friend class FFrameGraphPassBuilder;

        enum class EFrameGraphResourceType : u8 {
            Texture,
            Buffer
        };

        struct FRdgResourceAccess {
            EFrameGraphResourceType   mType       = EFrameGraphResourceType::Texture;
            u32                       mResourceId = 0;
            Rhi::ERhiResourceState    mState      = Rhi::ERhiResourceState::Unknown;
            bool                      mIsWrite    = false;
            bool                      mHasRange   = false;
            Rhi::FRhiTextureViewRange mRange;
        };

        struct FRdgTextureEntry {
            FFrameGraphTextureDesc mDesc;
            Rhi::FRhiTextureRef    mTexture;
            Rhi::FRhiTexture*      mExternal         = nullptr;
            bool                   mIsExternal       = false;
            bool                   mIsExternalOutput = false;
            Rhi::ERhiResourceState mFinalState       = Rhi::ERhiResourceState::Unknown;
        };

        struct FRdgBufferEntry {
            FFrameGraphBufferDesc  mDesc;
            Rhi::FRhiBufferRef     mBuffer;
            Rhi::FRhiBuffer*       mExternal         = nullptr;
            bool                   mIsExternal       = false;
            bool                   mIsExternalOutput = false;
            Rhi::ERhiResourceState mFinalState       = Rhi::ERhiResourceState::Unknown;
        };

        struct FRdgSRVEntry {
            bool                            mIsTexture  = true;
            u32                             mResourceId = 0;
            Rhi::FRhiShaderResourceViewDesc mDesc;
            Rhi::FRhiShaderResourceViewRef  mView;
        };

        struct FRdgUAVEntry {
            bool                             mIsTexture  = true;
            u32                              mResourceId = 0;
            Rhi::FRhiUnorderedAccessViewDesc mDesc;
            Rhi::FRhiUnorderedAccessViewRef  mView;
        };

        struct FRdgRTVEntry {
            u32                           mResourceId = 0;
            Rhi::FRhiRenderTargetViewDesc mDesc;
            Rhi::FRhiRenderTargetViewRef  mView;
        };

        struct FRdgDSVEntry {
            u32                           mResourceId = 0;
            Rhi::FRhiDepthStencilViewDesc mDesc;
            Rhi::FRhiDepthStencilViewRef  mView;
        };

        struct FRdgPass {
            FFrameGraphPassDesc              mDesc;
            TVector<FRdgResourceAccess>      mAccesses;
            TVector<FRdgRenderTargetBinding> mRenderTargets;
            bool                             mHasDepthStencil = false;
            FRdgDepthStencilBinding          mDepthStencil;
            bool                             mHasSideEffect = false;

            void*                            mPassData = nullptr;
            void (*mDestroyPassData)(void* data)       = nullptr;

            void* mExecuteUserData                       = nullptr;
            void (*mExecute)(Rhi::FRhiCmdContext& ctx, const FFrameGraphPassResources& res,
                const void* passData, void* executeData) = nullptr;
            void (*mDestroyExecute)(void* executeData)   = nullptr;

            TVector<Rhi::FRhiRenderPassColorAttachment> mCompiledColorAttachments;
            Rhi::FRhiRenderPassDepthStencilAttachment   mCompiledDepthAttachment;
            bool                                        mHasCompiledDepth = false;
        };

        template <typename PassData> static void DestroyPassData(void* data) {
            delete static_cast<PassData*>(data); // NOLINT
        }

        template <typename ExecuteFunc> static void DestroyExecute(void* executeData) {
            delete static_cast<ExecuteFunc*>(executeData); // NOLINT
        }

        template <typename PassData, typename ExecuteFunc>
        static void ExecuteWithData(Rhi::FRhiCmdContext& ctx, const FFrameGraphPassResources& res,
            const void* passData, void* executeData) {
            auto* exec = static_cast<ExecuteFunc*>(executeData);
            if constexpr (requires {
                              (*exec)(ctx, res, *static_cast<const PassData*>(passData));
                          }) {
                (*exec)(ctx, res, *static_cast<const PassData*>(passData));
            } else if constexpr (requires { (*exec)(ctx, res); }) {
                (*exec)(ctx, res);
            } else {
                static_assert(sizeof(ExecuteFunc) == 0,
                    "ExecuteFunc must accept (FRhiCmdContext&, const FFrameGraphPassResources&, const PassData&) "
                    "or (FRhiCmdContext&, const FFrameGraphPassResources&)");
            }
        }

        auto AllocatePass(const FFrameGraphPassDesc& desc) -> u32;

        void ResetGraph();

        auto CreateTextureInternal(const FFrameGraphTextureDesc& desc) -> FFrameGraphTextureRef;
        auto CreateBufferInternal(const FFrameGraphBufferDesc& desc) -> FFrameGraphBufferRef;

        auto CreateSRVInternal(FFrameGraphTextureRef tex,
            const Rhi::FRhiShaderResourceViewDesc&   desc) -> FFrameGraphSRVRef;
        auto CreateUAVInternal(FFrameGraphTextureRef tex,
            const Rhi::FRhiUnorderedAccessViewDesc&  desc) -> FFrameGraphUAVRef;
        auto CreateSRVInternal(FFrameGraphBufferRef buf,
            const Rhi::FRhiShaderResourceViewDesc&  desc) -> FFrameGraphSRVRef;
        auto CreateUAVInternal(FFrameGraphBufferRef buf,
            const Rhi::FRhiUnorderedAccessViewDesc& desc) -> FFrameGraphUAVRef;
        auto CreateRTVInternal(FFrameGraphTextureRef tex, const Rhi::FRhiRenderTargetViewDesc& desc)
            -> FFrameGraphRTVRef;
        auto CreateDSVInternal(FFrameGraphTextureRef tex, const Rhi::FRhiDepthStencilViewDesc& desc)
            -> FFrameGraphDSVRef;

        void RegisterTextureAccess(u32 passIndex, FFrameGraphTextureRef tex,
            Rhi::ERhiResourceState state, bool isWrite, const Rhi::FRhiTextureViewRange* range);
        void RegisterBufferAccess(
            u32 passIndex, FFrameGraphBufferRef buf, Rhi::ERhiResourceState state, bool isWrite);

        void SetRenderTargetsInternal(u32 passIndex, const FRdgRenderTargetBinding* RTVs,
            u32 RTVCount, const FRdgDepthStencilBinding* DSV);
        void SetExternalOutputInternal(
            u32 passIndex, FFrameGraphTextureRef tex, Rhi::ERhiResourceState finalState);
        void               SetSideEffectInternal(u32 passIndex);

        [[nodiscard]] auto ResolveTexture(FFrameGraphTextureRef ref) const -> Rhi::FRhiTexture*;
        [[nodiscard]] auto ResolveBuffer(FFrameGraphBufferRef ref) const -> Rhi::FRhiBuffer*;
        [[nodiscard]] auto ResolveSRV(FFrameGraphSRVRef ref) const -> Rhi::FRhiShaderResourceView*;
        [[nodiscard]] auto ResolveUAV(FFrameGraphUAVRef ref) const -> Rhi::FRhiUnorderedAccessView*;
        [[nodiscard]] auto ResolveRTV(FFrameGraphRTVRef ref) const -> Rhi::FRhiRenderTargetView*;
        [[nodiscard]] auto ResolveDSV(FFrameGraphDSVRef ref) const -> Rhi::FRhiDepthStencilView*;

    private:
        Rhi::FRhiDevice*          mDevice     = nullptr;
        u64                       mFrameIndex = 0;
        bool                      mInFrame    = false;
        bool                      mCompiled   = false;

        TVector<FRdgTextureEntry> mTextures;
        TVector<FRdgBufferEntry>  mBuffers;
        TVector<FRdgSRVEntry>     mSRVs;
        TVector<FRdgUAVEntry>     mUAVs;
        TVector<FRdgRTVEntry>     mRTVs;
        TVector<FRdgDSVEntry>     mDSVs;
        TVector<FRdgPass>         mPasses;
    };

} // namespace AltinaEngine::RenderCore
