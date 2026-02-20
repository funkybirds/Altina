#pragma once

#include "Asset/AssetLoader.h"
#include "Container/String.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::FNativeString;
    using Container::FNativeStringView;

    class AE_ASSET_API FShaderAsset final : public IAsset {
    public:
        FShaderAsset(u32 language, FNativeString source);

        [[nodiscard]] auto GetLanguage() const noexcept -> u32 { return mLanguage; }
        [[nodiscard]] auto GetSource() const noexcept -> FNativeStringView {
            return { mSource.GetData(), mSource.Length() };
        }

    private:
        u32          mLanguage = 0U;
        FNativeString mSource;
    };
} // namespace AltinaEngine::Asset
