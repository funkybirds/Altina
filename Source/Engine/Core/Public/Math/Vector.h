#pragma once

#include "Types/Aliases.h"
#include "Types/Concepts.h"

namespace AltinaEngine::Core::Math
{

    template <IScalar T, u32 ComponentCount> struct AE_CORE_API TVector
    {
        T                                Components[ComponentCount]{};

        [[nodiscard]] constexpr T&       operator[](u32 Index) noexcept { return Components[Index]; }
        [[nodiscard]] constexpr const T& operator[](u32 Index) const noexcept { return Components[Index]; }
    };

    using FVector2f = TVector<f32, 2U>;
    using FVector3f = TVector<f32, 3U>;
    using FVector4f = TVector<f32, 4U>;

    using FVector2i = TVector<i32, 2U>;
    using FVector3i = TVector<i32, 3U>;
    using FVector4i = TVector<i32, 4U>;

} // namespace AltinaEngine::Core::Math
