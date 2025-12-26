#include "../../Public/Math/Common.h"
#include <cmath>

namespace AltinaEngine::Core::Math
{
    namespace Details
    {
        AE_CORE_API f32 SinF(f32 radians) noexcept { return static_cast<f32>(::std::sin(static_cast<float>(radians))); }

        AE_CORE_API f64 SinD(f64 radians) noexcept { return ::sin(static_cast<double>(radians)); }

        AE_CORE_API f32 CosF(f32 radians) noexcept { return static_cast<f32>(::std::cos(static_cast<float>(radians))); }

        AE_CORE_API f64 CosD(f64 radians) noexcept { return ::cos(static_cast<double>(radians)); }

        AE_CORE_API f32 SqrtF(f32 value) noexcept { return static_cast<f32>(::std::sqrt(static_cast<float>(value))); }

        AE_CORE_API f64 SqrtD(f64 value) noexcept { return ::sqrt(static_cast<double>(value)); }
    } // namespace Details

} // namespace AltinaEngine::Core::Math
