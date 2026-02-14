#include "../../Public/Math/Common.h"
#include <cmath>

namespace AltinaEngine::Core::Math::Details {
    AE_CORE_API auto SinF(f32 radians) noexcept -> f32 { return sin(radians); }

    AE_CORE_API auto SinD(f64 radians) noexcept -> f64 { return sin(radians); }

    AE_CORE_API auto CosF(f32 radians) noexcept -> f32 { return cos(radians); }

    AE_CORE_API auto CosD(f64 radians) noexcept -> f64 { return cos(radians); }

    AE_CORE_API auto AsinF(f32 radians) noexcept -> f32 { return asin(radians); }

    AE_CORE_API auto AsinD(f64 radians) noexcept -> f64 { return asin(radians); }

    AE_CORE_API auto AcosF(f32 radians) noexcept -> f32 { return acos(radians); }

    AE_CORE_API auto AcosD(f64 radians) noexcept -> f64 { return acos(radians); }

    AE_CORE_API auto AtanF(f32 radians) noexcept -> f32 { return atan(radians); }

    AE_CORE_API auto AtanD(f64 radians) noexcept -> f64 { return atan(radians); }

    AE_CORE_API auto Atan2F(f32 y, f32 x) noexcept -> f32 { return atan2(y, x); }

    AE_CORE_API auto Atan2D(f64 y, f64 x) noexcept -> f64 { return atan2(y, x); }

    AE_CORE_API auto SqrtF(f32 value) noexcept -> f32 { return sqrt(value); }

    AE_CORE_API auto SqrtD(f64 value) noexcept -> f64 { return sqrt(value); }
} // namespace AltinaEngine::Core::Math::Details
