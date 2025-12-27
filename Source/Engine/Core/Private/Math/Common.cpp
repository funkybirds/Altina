#include "../../Public/Math/Common.h"
#include <cmath>

namespace AltinaEngine::Core::Math::Details
{
    AE_CORE_API auto SinF(f32 radians) noexcept -> f32 { return sin(radians); }

    AE_CORE_API auto SinD(f64 radians) noexcept -> f64 { return sin(radians); }

    AE_CORE_API auto CosF(f32 radians) noexcept -> f32 { return cos(radians); }

    AE_CORE_API auto CosD(f64 radians) noexcept -> f64 { return cos(radians); }

    AE_CORE_API auto SqrtF(f32 value) noexcept -> f32 { return sqrt(value); }

    AE_CORE_API auto SqrtD(f64 value) noexcept -> f64 { return sqrt(value); }
} // namespace AltinaEngine::Core::Math::Details
