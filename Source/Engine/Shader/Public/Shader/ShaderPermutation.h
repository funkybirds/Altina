#pragma once

#include "Types/Aliases.h"

namespace AltinaEngine::Shader {

    struct FShaderPermutationId {
        u64                          mHash = 0ULL;

        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool { return mHash != 0ULL; }

        friend constexpr auto        operator==(const FShaderPermutationId& lhs,
            const FShaderPermutationId& rhs) noexcept -> bool = default;
    };

} // namespace AltinaEngine::Shader
