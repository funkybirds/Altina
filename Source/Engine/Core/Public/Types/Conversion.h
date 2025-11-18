#pragma once

#include "Aliases.h"
#include "Concepts.h"
#include "../Base/AltinaBase.h"

#include <cstring>

namespace AltinaEngine
{

    template <typename TDst, typename TSrc>
        requires(ISameSizeAs<TDst, TSrc>)
    [[nodiscard]] AE_FORCEINLINE constexpr TDst BitCast(TSrc Value) noexcept
    {
        TDst Dst;
        std::memcpy(&Dst, &Value, sizeof(TDst));
        return Dst;
    }

} // namespace AltinaEngine