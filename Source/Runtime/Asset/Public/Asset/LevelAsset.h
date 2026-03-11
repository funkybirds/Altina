#pragma once

#include "Asset/AssetLoader.h"
#include "Container/Vector.h"

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::TVector;

    class AE_ASSET_API FLevelAsset final : public IAsset {
    public:
        FLevelAsset(u32 encoding, TVector<u8> payload);

        [[nodiscard]] auto GetEncoding() const noexcept -> u32 { return mEncoding; }
        [[nodiscard]] auto GetPayload() const noexcept -> const TVector<u8>& { return mPayload; }

    private:
        u32         mEncoding = 0;
        TVector<u8> mPayload{};
    };
} // namespace AltinaEngine::Asset
