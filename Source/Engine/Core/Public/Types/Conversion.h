#pragma once

#include "Aliases.h"
#include "Concepts.h"
#include "../Base/AltinaBase.h"
#include "Platform/PlatformIntrinsic.h"

namespace AltinaEngine
{
    template <ITriviallyConstructible TDst, typename TSrc>
        requires(ISameSizeAs<TDst, TSrc>)
    [[nodiscard]] AE_FORCEINLINE constexpr auto BitCast(const TSrc& Value) noexcept -> TDst
    {
        TDst dst;
        Core::Platform::Generic::Memcpy(&dst, &Value, sizeof(TDst));
        return dst;
    }

} // namespace AltinaEngine