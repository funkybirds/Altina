#pragma once

#include "Container/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Shader {
    using Core::Container::TVector;

    enum class EShaderStage : u8 {
        Vertex = 0,
        Pixel,
        Compute,
        Geometry,
        Hull,
        Domain,
        Mesh,
        Amplification,
        Library
    };

    struct FShaderBytecode {
        TVector<u8> mData;

        [[nodiscard]] auto IsEmpty() const noexcept -> bool { return mData.IsEmpty(); }
        [[nodiscard]] auto Data() const noexcept -> const u8* { return mData.Data(); }
        [[nodiscard]] auto Size() const noexcept -> usize { return mData.Size(); }
    };
} // namespace AltinaEngine::Shader
