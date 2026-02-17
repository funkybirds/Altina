#pragma once

#include "ShaderAPI.h"
#include "ShaderTypes.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Container/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Shader {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::FStringView;
    using Container::TVector;

    struct FShaderPermutationId {
        u64                          mHash = 0ULL;

        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool { return mHash != 0ULL; }

        friend constexpr auto        operator==(const FShaderPermutationId& lhs,
            const FShaderPermutationId& rhs) noexcept -> bool = default;
    };

    enum class EShaderPermutationValueType : u8 {
        Bool = 0,
        Int,
        Enum
    };

    enum class EShaderPermutationDomain : u8 {
        Multi = 0,
        Feature
    };

    struct FShaderPermutationDimension {
        FString                     mName;
        EShaderPermutationValueType mType   = EShaderPermutationValueType::Bool;
        EShaderPermutationDomain    mDomain = EShaderPermutationDomain::Multi;
        i32                         mDefaultValue = 0;
        i32                         mMinValue     = 0;
        i32                         mMaxValue     = 0;
        TVector<i32>                mEnumValues;
    };

    struct FShaderPermutationLayout {
        TVector<FShaderPermutationDimension> mDimensions;
    };

    struct FShaderPermutationValues {
        TVector<i32> mValues;
    };

    struct FShaderBuiltinDefinition {
        FString                     mName;
        EShaderPermutationValueType mType = EShaderPermutationValueType::Bool;
        i32                         mDefaultValue = 0;
    };

    struct FShaderBuiltinLayout {
        TVector<FShaderBuiltinDefinition> mBuiltins;
    };

    struct FShaderBuiltinValues {
        TVector<i32> mValues;
    };

    enum class EShaderRasterFillMode : u8 {
        Solid = 0,
        Wireframe
    };

    enum class EShaderRasterCullMode : u8 {
        None = 0,
        Front,
        Back
    };

    enum class EShaderRasterFrontFace : u8 {
        CCW = 0,
        CW
    };

    struct FShaderRasterState {
        EShaderRasterFillMode  mFillMode            = EShaderRasterFillMode::Solid;
        EShaderRasterCullMode  mCullMode            = EShaderRasterCullMode::Back;
        EShaderRasterFrontFace mFrontFace           = EShaderRasterFrontFace::CCW;
        i32                    mDepthBias           = 0;
        f32                    mDepthBiasClamp      = 0.0f;
        f32                    mSlopeScaledDepthBias = 0.0f;
        bool                   mDepthClip           = true;
        bool                   mConservativeRaster  = false;
    };

    [[nodiscard]] AE_SHADER_API auto BuildShaderPermutationId(FStringView shaderPath,
        FStringView entryPoint, EShaderStage stage, const FShaderPermutationLayout& layout,
        const FShaderPermutationValues& values, const FShaderBuiltinLayout* builtinLayout = nullptr,
        const FShaderBuiltinValues* builtinValues = nullptr) noexcept -> FShaderPermutationId;

} // namespace AltinaEngine::Shader
