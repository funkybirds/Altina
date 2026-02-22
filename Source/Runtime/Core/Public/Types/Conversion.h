#pragma once

#include "Aliases.h"
#include "Concepts.h"
#include "../Base/AltinaBase.h"
#include "Platform/PlatformIntrinsic.h"

namespace AltinaEngine {
    template <CTriviallyConstructible TDst, typename TSrc>
        requires(CSameSizeAs<TDst, TSrc>)
    [[nodiscard]] AE_FORCEINLINE constexpr auto BitCast(const TSrc& Value) noexcept -> TDst {
        TDst dst;
        Core::Platform::Generic::Memcpy(&dst, &Value, sizeof(TDst));
        return dst;
    }

} // namespace AltinaEngine