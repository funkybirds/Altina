#pragma once

#include "ShaderCompilerAPI.h"
#include "ShaderCompiler/ShaderCompileTypes.h"
#include "Shader/ShaderPermutation.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Container/Vector.h"

namespace AltinaEngine::ShaderCompiler {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::FStringView;
    using Container::TVector;
    using Shader::FShaderBuiltinLayout;
    using Shader::FShaderBuiltinValues;
    using Shader::FShaderPermutationLayout;
    using Shader::FShaderPermutationValues;

    enum class EShaderPermutationRuleOperator : u8 {
        Add,
        Subtract,
        Multiply,
        Divide,
        Modulo,
        Equal,
        NotEqual,
        Less,
        LessEqual,
        Greater,
        GreaterEqual,
        LogicalAnd,
        LogicalOr,
        LogicalNot,
        UnaryNegate,
        UnaryPlus
    };

    struct FShaderPermutationRuleExpression {
        enum class EKind : u8 {
            Literal,
            Identifier,
            Unary,
            Binary
        };

        EKind                         mKind = EKind::Literal;
        EShaderPermutationRuleOperator mOperator = EShaderPermutationRuleOperator::Add;
        i32                           mLiteral   = 0;
        FString                       mIdentifier;
        i32                           mLeftIndex  = -1;
        i32                           mRightIndex = -1;
    };

    struct FShaderPermutationRuleSet {
        struct FLetRule {
            FString mName;
            i32     mExpressionIndex = -1;
        };

        TVector<FLetRule>                 mLets;
        TVector<i32>                      mRequires;
        TVector<FShaderPermutationRuleExpression> mExpressions;
    };

    struct FShaderPermutationParseResult {
        bool                       mSucceeded = false;
        FString                    mDiagnostics;
        FShaderPermutationLayout   mPermutationLayout;
        FShaderBuiltinLayout       mBuiltinLayout;
        FShaderPermutationRuleSet  mRules;
    };

    AE_SHADER_COMPILER_API auto ParseShaderPermutationSource(FStringView source,
        FShaderPermutationParseResult& out) -> bool;

    AE_SHADER_COMPILER_API auto BuildDefaultPermutationValues(
        const FShaderPermutationLayout& layout) -> FShaderPermutationValues;

    AE_SHADER_COMPILER_API auto BuildDefaultBuiltinValues(
        const FShaderBuiltinLayout& layout) -> FShaderBuiltinValues;

    AE_SHADER_COMPILER_API auto ExpandMultiPermutationValues(
        const FShaderPermutationLayout& layout, TVector<FShaderPermutationValues>& outValues,
        usize maxPermutations = 1024) -> bool;

    AE_SHADER_COMPILER_API auto EvaluateShaderPermutationRules(
        const FShaderPermutationRuleSet& rules, const FShaderPermutationLayout& layout,
        const FShaderPermutationValues& values, const FShaderBuiltinLayout* builtinLayout,
        const FShaderBuiltinValues* builtinValues, FString* outDiagnostics = nullptr) -> bool;

    AE_SHADER_COMPILER_API auto BuildPermutationDefines(
        const FShaderPermutationLayout& layout, const FShaderPermutationValues& values,
        FStringView prefix = TEXT("AE_PERM_")) -> TVector<FShaderMacro>;

    AE_SHADER_COMPILER_API auto BuildBuiltinDefines(const FShaderBuiltinLayout& layout,
        const FShaderBuiltinValues& values, FStringView prefix = TEXT("AE_BUILTIN_"))
        -> TVector<FShaderMacro>;
} // namespace AltinaEngine::ShaderCompiler
