#pragma once

#include "Base/CoreAPI.h"
#include "Container/HashMap.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Container/Vector.h"
#include "Types/Aliases.h"
#include "Utility/Json.h"

namespace AltinaEngine {
    struct FStartupParameters;
}

namespace AltinaEngine::Core::Utility::EngineConfig {
    using Container::FNativeString;
    using Container::FNativeStringView;
    using Container::FString;
    using Container::FStringView;
    using Container::THashMap;
    using Container::TVector;

    class AE_CORE_API FConfigCollection {
    public:
        FConfigCollection() = default;

        [[nodiscard]] auto ParseJsonConfig(FNativeStringView jsonText) -> bool;
        void               ApplyStartupParamOverrides(const FStartupParameters& startupParameters);
        void               Clear() noexcept;

        [[nodiscard]] auto GetBool(FStringView path) const noexcept -> bool;
        [[nodiscard]] auto GetString(FStringView path) const -> FString;
        [[nodiscard]] auto GetInt32(FStringView path) const noexcept -> i32;
        [[nodiscard]] auto GetInt64(FStringView path) const noexcept -> i64;
        [[nodiscard]] auto GetFloat32(FStringView path) const noexcept -> f32;
        [[nodiscard]] auto GetFloat64(FStringView path) const noexcept -> f64;
        [[nodiscard]] auto GetUint32(FStringView path) const noexcept -> u32;
        [[nodiscard]] auto GetUint64(FStringView path) const noexcept -> u64;
        [[nodiscard]] auto GetStringArray(FStringView path) const -> TVector<FString>;

    private:
        enum class EOverrideType : u8 {
            Bool,
            String,
            StringArray,
        };

        struct FOverrideValue {
            EOverrideType    Type      = EOverrideType::String;
            bool             BoolValue = false;
            FString          StringValue;
            TVector<FString> StringArrayValue;
        };

        [[nodiscard]] auto FindOverride(FStringView path) const -> const FOverrideValue*;

    private:
        Json::FJsonDocument               mDocument;
        THashMap<FString, FOverrideValue> mOverrides;
    };

    AE_CORE_API auto GetGlobalConfig() noexcept -> const FConfigCollection&;
    AE_CORE_API void InitializeGlobalConfig(const FStartupParameters& startupParameters);

} // namespace AltinaEngine::Core::Utility::EngineConfig
