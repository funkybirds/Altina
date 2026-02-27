#include "Shader/ShaderPreset.h"

namespace AltinaEngine::RenderCore {
    FShaderPresetRegistry::FPresetMap FShaderPresetRegistry::sPresets{};

    auto FShaderPresetRegistry::FStringHash::operator()(const FString& value) const noexcept
        -> usize {
        constexpr usize kOffset = 1469598103934665603ULL;
        constexpr usize kPrime  = 1099511628211ULL;
        usize           hash    = kOffset;
        const auto      view    = value.ToView();
        for (usize i = 0U; i < view.Length(); ++i) {
            hash ^= static_cast<usize>(static_cast<FStringView::TUnsigned>(view[i]));
            hash *= kPrime;
        }
        return hash;
    }

    auto FShaderPresetRegistry::FStringEqual::operator()(
        const FString& a, const FString& b) const noexcept -> bool {
        return a == b;
    }

    auto FShaderPresetRegistry::RegisterPreset(FStringView name, FStringView sourcePath) -> bool {
        if (name.IsEmpty() || sourcePath.IsEmpty()) {
            return false;
        }
        auto nameStr      = FString(name);
        auto pathStr      = FString(sourcePath);
        sPresets[nameStr] = pathStr;
        return true;
    }

    auto FShaderPresetRegistry::FindPreset(FStringView name) -> const FString* {
        if (name.IsEmpty()) {
            return nullptr;
        }
        auto       nameStr = FString(name);
        const auto it      = sPresets.find(nameStr);
        if (it == sPresets.end()) {
            return nullptr;
        }
        return &it->second;
    }

    void FShaderPresetRegistry::Clear() noexcept { sPresets.clear(); }

    void InitCommonShaders() {
        static bool sInitialized = false;
        if (sInitialized) {
            return;
        }

        (void)FShaderPresetRegistry::RegisterPreset(TEXT("Deferred/Lit/PBR.Standard"),
            TEXT("Shader/Deferred/Materials/Lit/PBR.Standard.hlsl"));

        (void)FShaderPresetRegistry::RegisterPreset(TEXT("Deferred/Unlit/OrbitLine.Billboard"),
            TEXT("Shader/Deferred/OrbitLine.Billboard.hlsl"));

        sInitialized = true;
    }
} // namespace AltinaEngine::RenderCore
