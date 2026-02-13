#pragma once

#include "Asset/AssetTypes.h"
#include "Container/SmartPtr.h"

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::TShared;

    class AE_ASSET_API IAsset {
    public:
        virtual ~IAsset() = default;
    };

    class AE_ASSET_API IAssetStream {
    public:
        virtual ~IAssetStream() = default;

        [[nodiscard]] virtual auto Size() const noexcept -> usize                    = 0;
        [[nodiscard]] virtual auto Tell() const noexcept -> usize                    = 0;
        virtual void               Seek(usize offset) noexcept                       = 0;
        virtual auto               Read(void* outBuffer, usize bytesToRead) -> usize = 0;
    };

    class AE_ASSET_API IAssetLoader {
    public:
        virtual ~IAssetLoader() = default;

        [[nodiscard]] virtual auto CanLoad(EAssetType type) const noexcept -> bool         = 0;
        virtual auto Load(const FAssetDesc& desc, IAssetStream& stream) -> TShared<IAsset> = 0;
    };

} // namespace AltinaEngine::Asset
