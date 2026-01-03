#pragma once
#include "Types/Traits.h"

#define AE_RTTI_ENABLED 1

#ifdef AE_RTTI_ENABLED
    #include <typeinfo>
#endif

namespace AltinaEngine {
#ifdef AE_RTTI_ENABLED
    using FTypeInfo = std::type_info; // NOLINT
#else
    using FTypeInfo = void; // NOLINT
#endif

    template <typename T> [[nodiscard]] auto GetRttiTypeInfo() -> const FTypeInfo& {
        return typeid(T);
    }
    template <typename T> [[nodiscard]] auto GetRttiTypeHash() -> usize {
        return typeid(T).hash_code();
    }

    [[nodiscard]] inline auto GetRttiTypeObjectHash(const FTypeInfo& stdTypeInfo) -> usize {
        return stdTypeInfo.hash_code();
    }
} // namespace AltinaEngine