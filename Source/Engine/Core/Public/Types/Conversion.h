#pragma once

#include "Aliases.h"
#include "Concepts.h"
#include "../Base/AltinaBase.h"

#include <cstring>

namespace AltinaEngine
{

    template <typename TDst, typename TSrc>
        requires(ISameSizeAs<TDst, TSrc>)
    [[nodiscard]] AE_FORCEINLINE constexpr auto BitCast(TSrc Value) noexcept -> TDst
    {
        TDst dst;
        std::memcpy(&dst, &Value, sizeof(TDst));
        return dst;
    }

} // namespace AltinaEngine