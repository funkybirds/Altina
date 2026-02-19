#include "TestHarness.h"

#include "ShaderCompiler/ShaderPermutationParser.h"
#include "Shader/ShaderPermutation.h"

namespace {
    using AltinaEngine::TChar;
    using AltinaEngine::Core::Container::TVector;
    using AltinaEngine::Shader::BuildShaderPermutationId;
    using AltinaEngine::Shader::EShaderRasterCullMode;
    using AltinaEngine::Shader::EShaderRasterFillMode;
    using AltinaEngine::Shader::EShaderRasterFrontFace;
    using AltinaEngine::Shader::EShaderStage;
    using AltinaEngine::Shader::FShaderPermutationId;
    using AltinaEngine::ShaderCompiler::BuildBuiltinDefines;
    using AltinaEngine::ShaderCompiler::BuildDefaultBuiltinValues;
    using AltinaEngine::ShaderCompiler::BuildDefaultPermutationValues;
    using AltinaEngine::ShaderCompiler::BuildPermutationDefines;
    using AltinaEngine::ShaderCompiler::EvaluateShaderPermutationRules;
    using AltinaEngine::ShaderCompiler::ExpandMultiPermutationValues;
    using AltinaEngine::ShaderCompiler::FShaderPermutationParseResult;
    using AltinaEngine::ShaderCompiler::ParseShaderPermutationSource;

    auto FindDimensionIndex(const AltinaEngine::Shader::FShaderPermutationLayout& layout,
        const TChar* name) -> AltinaEngine::i32 {
        for (AltinaEngine::usize i = 0; i < layout.mDimensions.Size(); ++i) {
            if (layout.mDimensions[i].mName == name) {
                return static_cast<AltinaEngine::i32>(i);
            }
        }
        return -1;
    }

    auto FindBuiltinIndex(const AltinaEngine::Shader::FShaderBuiltinLayout& layout,
        const TChar* name) -> AltinaEngine::i32 {
        for (AltinaEngine::usize i = 0; i < layout.mBuiltins.Size(); ++i) {
            if (layout.mBuiltins[i].mName == name) {
                return static_cast<AltinaEngine::i32>(i);
            }
        }
        return -1;
    }

    auto FindDefine(const TVector<AltinaEngine::ShaderCompiler::FShaderMacro>& defines,
        const TChar* name) -> const AltinaEngine::ShaderCompiler::FShaderMacro* {
        for (const auto& macro : defines) {
            if (macro.mName == name) {
                return &macro;
            }
        }
        return nullptr;
    }
} // namespace

TEST_CASE("ShaderCompiler.ShaderPermutation.ParseAndEvaluate") {
    const auto                    shaderSource = TEXT(R"(
// @altina perm {
//   USE_FOG: bool = 1 [multi]
//   SHADING_MODEL: enum {0,1,2} = 2 [multi]
//   NUM_LIGHTS: int [0..4] = 2 [feature]
// }
// @altina builtins {
//   AE_BUILTIN_REVERSEZ: bool;
//   AE_BUILTIN_DIRECTIONAL_LIGHT: bool;
// }
// @altina rules {
//   let HasFog = (USE_FOG == 1);
//   let UsePBR = (SHADING_MODEL == 2);
//   let NeedDirLight = AE_BUILTIN_DIRECTIONAL_LIGHT && (NUM_LIGHTS > 0);
//   require !(HasFog && UsePBR);
//   require (NUM_LIGHTS <= 3) || !UsePBR;
//   require !AE_BUILTIN_REVERSEZ || (SHADING_MODEL != 0);
// }
)");

    FShaderPermutationParseResult parsed;
    REQUIRE(ParseShaderPermutationSource(shaderSource, parsed));
    REQUIRE(parsed.mSucceeded);
    REQUIRE_EQ(parsed.mPermutationLayout.mDimensions.Size(), 3U);
    REQUIRE_EQ(parsed.mBuiltinLayout.mBuiltins.Size(), 2U);

    TVector<AltinaEngine::Shader::FShaderPermutationValues> combos;
    REQUIRE(ExpandMultiPermutationValues(parsed.mPermutationLayout, combos, 32));
    REQUIRE_EQ(combos.Size(), 6U);

    auto       values   = BuildDefaultPermutationValues(parsed.mPermutationLayout);
    auto       builtins = BuildDefaultBuiltinValues(parsed.mBuiltinLayout);

    const auto useFogIndex   = FindDimensionIndex(parsed.mPermutationLayout, TEXT("USE_FOG"));
    const auto shadingIndex  = FindDimensionIndex(parsed.mPermutationLayout, TEXT("SHADING_MODEL"));
    const auto lightsIndex   = FindDimensionIndex(parsed.mPermutationLayout, TEXT("NUM_LIGHTS"));
    const auto reverseZIndex = FindBuiltinIndex(parsed.mBuiltinLayout, TEXT("AE_BUILTIN_REVERSEZ"));
    REQUIRE(useFogIndex >= 0);
    REQUIRE(shadingIndex >= 0);
    REQUIRE(lightsIndex >= 0);
    REQUIRE(reverseZIndex >= 0);

    values.mValues[static_cast<AltinaEngine::usize>(useFogIndex)]     = 1;
    values.mValues[static_cast<AltinaEngine::usize>(shadingIndex)]    = 2;
    values.mValues[static_cast<AltinaEngine::usize>(lightsIndex)]     = 2;
    builtins.mValues[static_cast<AltinaEngine::usize>(reverseZIndex)] = 0;
    REQUIRE(!EvaluateShaderPermutationRules(
        parsed.mRules, parsed.mPermutationLayout, values, &parsed.mBuiltinLayout, &builtins));

    values.mValues[static_cast<AltinaEngine::usize>(useFogIndex)]  = 0;
    values.mValues[static_cast<AltinaEngine::usize>(shadingIndex)] = 2;
    values.mValues[static_cast<AltinaEngine::usize>(lightsIndex)]  = 4;
    REQUIRE(!EvaluateShaderPermutationRules(
        parsed.mRules, parsed.mPermutationLayout, values, &parsed.mBuiltinLayout, &builtins));

    values.mValues[static_cast<AltinaEngine::usize>(useFogIndex)]     = 0;
    values.mValues[static_cast<AltinaEngine::usize>(shadingIndex)]    = 1;
    values.mValues[static_cast<AltinaEngine::usize>(lightsIndex)]     = 4;
    builtins.mValues[static_cast<AltinaEngine::usize>(reverseZIndex)] = 1;
    REQUIRE(EvaluateShaderPermutationRules(
        parsed.mRules, parsed.mPermutationLayout, values, &parsed.mBuiltinLayout, &builtins));

    const auto permDefines    = BuildPermutationDefines(parsed.mPermutationLayout, values);
    const auto builtinDefines = BuildBuiltinDefines(parsed.mBuiltinLayout, builtins);
    REQUIRE_EQ(permDefines.Size(), parsed.mPermutationLayout.mDimensions.Size());
    REQUIRE_EQ(builtinDefines.Size(), parsed.mBuiltinLayout.mBuiltins.Size());

    const auto* fogDefine     = FindDefine(permDefines, TEXT("AE_PERM_USE_FOG"));
    const auto* reverseDefine = FindDefine(builtinDefines, TEXT("AE_BUILTIN_REVERSEZ"));
    REQUIRE(fogDefine != nullptr);
    REQUIRE(reverseDefine != nullptr);
}

TEST_CASE("ShaderCompiler.ShaderPermutation.RasterState") {
    const auto                    shaderSource = TEXT(R"(
// @altina raster_state {
//   fill = wireframe
//   cull = front
//   front_face = cw
//   depth_bias = 4
//   depth_bias_clamp = 1.5
//   slope_scaled_depth_bias = 0.25
//   depth_clip = false
//   conservative = true
// }
)");

    FShaderPermutationParseResult parsed;
    REQUIRE(ParseShaderPermutationSource(shaderSource, parsed));
    REQUIRE(parsed.mSucceeded);
    REQUIRE(parsed.mHasRasterState);
    REQUIRE(parsed.mRasterState.mFillMode == EShaderRasterFillMode::Wireframe);
    REQUIRE(parsed.mRasterState.mCullMode == EShaderRasterCullMode::Front);
    REQUIRE(parsed.mRasterState.mFrontFace == EShaderRasterFrontFace::CW);
    REQUIRE_EQ(parsed.mRasterState.mDepthBias, 4);
    REQUIRE_CLOSE(parsed.mRasterState.mDepthBiasClamp, 1.5f, 0.0001f);
    REQUIRE_CLOSE(parsed.mRasterState.mSlopeScaledDepthBias, 0.25f, 0.0001f);
    REQUIRE(!parsed.mRasterState.mDepthClip);
    REQUIRE(parsed.mRasterState.mConservativeRaster);
}

TEST_CASE("ShaderCompiler.ShaderPermutation.BuildId") {
    const auto                    shaderSource = TEXT(R"(
// @altina perm {
//   USE_FOG: bool = 0 [multi]
//   SHADING_MODEL: enum {0,1,2} = 1 [multi]
// }
// @altina builtins {
//   AE_BUILTIN_REVERSEZ: bool;
// }
)");

    FShaderPermutationParseResult parsed;
    REQUIRE(ParseShaderPermutationSource(shaderSource, parsed));

    auto       values   = BuildDefaultPermutationValues(parsed.mPermutationLayout);
    auto       builtins = BuildDefaultBuiltinValues(parsed.mBuiltinLayout);

    const auto fogIndex     = FindDimensionIndex(parsed.mPermutationLayout, TEXT("USE_FOG"));
    const auto reverseIndex = FindBuiltinIndex(parsed.mBuiltinLayout, TEXT("AE_BUILTIN_REVERSEZ"));
    REQUIRE(fogIndex >= 0);
    REQUIRE(reverseIndex >= 0);

    values.mValues[static_cast<AltinaEngine::usize>(fogIndex)]       = 0;
    builtins.mValues[static_cast<AltinaEngine::usize>(reverseIndex)] = 0;

    const auto idA = BuildShaderPermutationId(TEXT("TestShader"), TEXT("VSMain"),
        EShaderStage::Vertex, parsed.mPermutationLayout, values, &parsed.mBuiltinLayout, &builtins);
    const auto idB = BuildShaderPermutationId(TEXT("TestShader"), TEXT("VSMain"),
        EShaderStage::Vertex, parsed.mPermutationLayout, values, &parsed.mBuiltinLayout, &builtins);
    REQUIRE(idA.IsValid());
    REQUIRE_EQ(idA.mHash, idB.mHash);

    values.mValues[static_cast<AltinaEngine::usize>(fogIndex)] = 1;
    const auto idC = BuildShaderPermutationId(TEXT("TestShader"), TEXT("VSMain"),
        EShaderStage::Vertex, parsed.mPermutationLayout, values, &parsed.mBuiltinLayout, &builtins);
    REQUIRE(idC.IsValid());
    REQUIRE(idA.mHash != idC.mHash);
}
