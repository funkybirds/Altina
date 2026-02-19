#include "Shader/ShaderPermutation.h"

#include <type_traits>

namespace AltinaEngine::Shader {
    namespace {
        constexpr u64                  kFnvOffset = 14695981039346656037ULL;
        constexpr u64                  kFnvPrime  = 1099511628211ULL;

        template <typename CharT> void HashChars(u64& hash, const CharT* data, usize length) {
            if ((data == nullptr) || (length == 0U)) {
                return;
            }
            for (usize i = 0; i < length; ++i) {
                const auto ch = static_cast<u64>(static_cast<std::make_unsigned_t<CharT>>(data[i]));
                hash ^= ch;
                hash *= kFnvPrime;
            }
        }

        void HashString(u64& hash, FStringView text) {
            HashChars(hash, text.Data(), text.Length());
        }

        void HashNumber(u64& hash, i64 value) {
            const auto text = FString::ToString(value);
            HashChars(hash, text.GetData(), text.Length());
        }

        void HashSeparator(u64& hash, TChar separator) {
            const auto ch = static_cast<u64>(static_cast<std::make_unsigned_t<TChar>>(separator));
            hash ^= ch;
            hash *= kFnvPrime;
        }
    } // namespace

    auto BuildShaderPermutationId(FStringView shaderPath, FStringView entryPoint,
        EShaderStage stage, const FShaderPermutationLayout& layout,
        const FShaderPermutationValues& values, const FShaderBuiltinLayout* builtinLayout,
        const FShaderBuiltinValues* builtinValues) noexcept -> FShaderPermutationId {
        if (layout.mDimensions.Size() != values.mValues.Size()) {
            return {};
        }

        if ((builtinLayout != nullptr) && (builtinValues != nullptr)
            && (builtinLayout->mBuiltins.Size() != builtinValues->mValues.Size())) {
            return {};
        }

        u64 hash = kFnvOffset;
        HashString(hash, shaderPath);
        HashSeparator(hash, static_cast<TChar>('|'));
        HashString(hash, entryPoint);
        HashSeparator(hash, static_cast<TChar>('|'));
        HashNumber(hash, static_cast<i64>(stage));
        HashSeparator(hash, static_cast<TChar>('|'));

        const TChar kEq  = static_cast<TChar>('=');
        const TChar kSep = static_cast<TChar>(';');
        for (usize i = 0; i < layout.mDimensions.Size(); ++i) {
            const auto& dim = layout.mDimensions[i];
            HashString(hash, dim.mName.ToView());
            HashSeparator(hash, kEq);
            HashNumber(hash, values.mValues[i]);
            HashSeparator(hash, kSep);
        }

        if ((builtinLayout != nullptr) && (builtinValues != nullptr)) {
            for (usize i = 0; i < builtinLayout->mBuiltins.Size(); ++i) {
                const auto& builtin = builtinLayout->mBuiltins[i];
                HashString(hash, builtin.mName.ToView());
                HashSeparator(hash, kEq);
                HashNumber(hash, builtinValues->mValues[i]);
                HashSeparator(hash, kSep);
            }
        }

        return { hash };
    }

} // namespace AltinaEngine::Shader
