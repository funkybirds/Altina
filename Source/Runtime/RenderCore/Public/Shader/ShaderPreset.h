#pragma once

#include "RenderCoreAPI.h"

#include "Container/HashMap.h"
#include "Container/String.h"
#include "Container/StringView.h"

namespace AltinaEngine::RenderCore {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::FStringView;

    class AE_RENDER_CORE_API FShaderPresetRegistry final {
    public:
        static auto               RegisterPreset(FStringView name, FStringView sourcePath) -> bool;
        [[nodiscard]] static auto FindPreset(FStringView name) -> const FString*;
        static void               Clear() noexcept;

    private:
        struct FStringHash {
            auto operator()(const FString& value) const noexcept -> usize;
        };
        struct FStringEqual {
            auto operator()(const FString& a, const FString& b) const noexcept -> bool;
        };

        using FPresetMap = Container::THashMap<FString, FString, FStringHash, FStringEqual>;
        static FPresetMap sPresets;
    };

    AE_RENDER_CORE_API void InitCommonShaders();
} // namespace AltinaEngine::RenderCore
