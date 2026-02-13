#pragma once

#include "Base/CoreAPI.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Container/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Core::Utility::Json {
    using Container::FNativeString;
    using Container::FNativeStringView;
    using Container::TVector;

    enum class EJsonType : u8 {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object,
    };

    struct FJsonValue;

    struct FJsonPair {
        FNativeString Key;
        FJsonValue*   Value = nullptr;
    };

    struct AE_CORE_API FJsonValue {
        EJsonType            Type   = EJsonType::Null;
        double               Number = 0.0;
        bool                 Bool   = false;
        FNativeString        String;
        TVector<FJsonValue*> Array;
        TVector<FJsonPair>   Object;
    };

    class AE_CORE_API FJsonDocument {
    public:
        FJsonDocument() = default;
        ~FJsonDocument();

        FJsonDocument(const FJsonDocument&)                    = delete;
        auto operator=(const FJsonDocument&) -> FJsonDocument& = delete;

        FJsonDocument(FJsonDocument&& other) noexcept;
        auto               operator=(FJsonDocument&& other) noexcept -> FJsonDocument&;

        [[nodiscard]] auto Parse(FNativeStringView text) -> bool;
        void               Clear();

        [[nodiscard]] auto GetRoot() const noexcept -> const FJsonValue* { return mRoot; }
        [[nodiscard]] auto GetError() const noexcept -> FNativeStringView;

    private:
        void                 DestroyValues();

        FJsonValue*          mRoot = nullptr;
        TVector<FJsonValue*> mOwned;
        FNativeString        mError;
    };

    AE_CORE_API auto FindObjectValue(const FJsonValue& object, const char* key)
        -> const FJsonValue*;
    AE_CORE_API auto FindObjectValueInsensitive(const FJsonValue& object, const char* key)
        -> const FJsonValue*;
    AE_CORE_API auto GetStringValue(const FJsonValue* value, FNativeString& out) -> bool;
    AE_CORE_API auto GetNumberValue(const FJsonValue* value, double& out) -> bool;
    AE_CORE_API auto GetBoolValue(const FJsonValue* value, bool& out) -> bool;

} // namespace AltinaEngine::Core::Utility::Json
