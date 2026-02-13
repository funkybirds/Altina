#pragma once

#include "Asset/AssetBinary.h"
#include "Asset/AssetLoader.h"
#include "Container/Vector.h"

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::TVector;

    struct AE_ASSET_API FAudioRuntimeDesc {
        u32 Codec          = 0;
        u32 SampleFormat   = 0;
        u32 Channels       = 0;
        u32 SampleRate     = 0;
        u32 FrameCount     = 0;
        u32 FramesPerChunk = 0;
    };

    class AE_ASSET_API FAudioAsset final : public IAsset {
    public:
        FAudioAsset(FAudioRuntimeDesc desc, TVector<FAudioChunkDesc> chunks, TVector<u8> data);

        [[nodiscard]] auto GetDesc() const noexcept -> const FAudioRuntimeDesc& { return mDesc; }
        [[nodiscard]] auto GetChunks() const noexcept -> const TVector<FAudioChunkDesc>& {
            return mChunks;
        }
        [[nodiscard]] auto GetData() const noexcept -> const TVector<u8>& { return mData; }

    private:
        FAudioRuntimeDesc        mDesc{};
        TVector<FAudioChunkDesc> mChunks;
        TVector<u8>              mData;
    };

} // namespace AltinaEngine::Asset
