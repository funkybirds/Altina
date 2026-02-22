#pragma once

#include "Asset/AssetLoader.h"
#include "Container/String.h"
#include "Container/StringView.h"

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::FNativeString;
    using Container::FNativeStringView;

    class AE_ASSET_API FScriptAsset final : public IAsset {
    public:
        FScriptAsset(FNativeString assemblyPath, FNativeString typeName);

        [[nodiscard]] auto GetAssemblyPath() const noexcept -> FNativeStringView;
        [[nodiscard]] auto GetTypeName() const noexcept -> FNativeStringView;

    private:
        FNativeString mAssemblyPath;
        FNativeString mTypeName;
    };
} // namespace AltinaEngine::Asset
