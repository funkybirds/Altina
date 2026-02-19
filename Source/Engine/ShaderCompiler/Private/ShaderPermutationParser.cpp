
#include "ShaderCompiler/ShaderPermutationParser.h"

#include <cstdlib>
#include <cwchar>
#include <string>

#include "Container/HashMap.h"
#include "Types/Traits.h"

namespace AltinaEngine::ShaderCompiler {
    namespace Container = Core::Container;
    using Container::THashMap;
    using Container::TVector;
    using Shader::EShaderPermutationDomain;
    using Shader::EShaderPermutationValueType;
    using Shader::FShaderBuiltinDefinition;
    using Shader::FShaderPermutationDimension;
    using Shader::FShaderRasterState;

    namespace {
        auto ParseIntLiteral(const char* text, char** endPtr) -> long {
            return std::strtol(text, endPtr, 10);
        }

        auto ParseIntLiteral(const wchar_t* text, wchar_t** endPtr) -> long {
            return std::wcstol(text, endPtr, 10);
        }

        auto ParseFloatLiteral(const char* text, char** endPtr) -> double {
            return std::strtod(text, endPtr);
        }

        auto ParseFloatLiteral(const wchar_t* text, wchar_t** endPtr) -> double {
            return std::wcstod(text, endPtr);
        }

        auto IsWhitespace(TChar ch) -> bool {
            return (ch == static_cast<TChar>(' ')) || (ch == static_cast<TChar>('\t'))
                || (ch == static_cast<TChar>('\r')) || (ch == static_cast<TChar>('\n'));
        }

        auto IsDigit(TChar ch) -> bool {
            return (ch >= static_cast<TChar>('0')) && (ch <= static_cast<TChar>('9'));
        }

        auto IsAlpha(TChar ch) -> bool {
            return ((ch >= static_cast<TChar>('a')) && (ch <= static_cast<TChar>('z')))
                || ((ch >= static_cast<TChar>('A')) && (ch <= static_cast<TChar>('Z')))
                || (ch == static_cast<TChar>('_'));
        }

        auto IsIdentifierStart(TChar ch) -> bool { return IsAlpha(ch); }

        auto IsIdentifierChar(TChar ch) -> bool { return IsAlpha(ch) || IsDigit(ch); }

        auto TrimView(FStringView view) -> FStringView {
            if (view.IsEmpty()) {
                return {};
            }
            usize       start  = 0U;
            const usize length = view.Length();
            while (start < length && IsWhitespace(view[start])) {
                ++start;
            }
            if (start == length) {
                return {};
            }
            usize end = length;
            while (end > start && IsWhitespace(view[end - 1])) {
                --end;
            }
            return view.Substr(start, end - start);
        }

        auto TrimLineTerminator(FStringView view) -> FStringView {
            auto trimmed = TrimView(view);
            if (trimmed.IsEmpty()) {
                return trimmed;
            }
            if (trimmed.EndsWith(FStringView(TEXT(";")))) {
                trimmed = TrimView(trimmed.Substr(0, trimmed.Length() - 1U));
            }
            return trimmed;
        }

        auto ParseBoolValue(FStringView value, bool& out) -> bool {
            if (value == FStringView(TEXT("true")) || value == FStringView(TEXT("1"))) {
                out = true;
                return true;
            }
            if (value == FStringView(TEXT("false")) || value == FStringView(TEXT("0"))) {
                out = false;
                return true;
            }
            return false;
        }

        auto ParseIntValue(FStringView value, i32& out) -> bool {
            const auto trimmed = TrimView(value);
            if (trimmed.IsEmpty()) {
                return false;
            }
            std::basic_string<TChar> temp(trimmed.Data(), trimmed.Data() + trimmed.Length());
            TChar*                   endPtr = nullptr;
            const long               parsed = ParseIntLiteral(temp.c_str(), &endPtr);
            if (endPtr == temp.c_str() || *endPtr != static_cast<TChar>(0)) {
                return false;
            }
            out = static_cast<i32>(parsed);
            return true;
        }

        auto ParseFloatValue(FStringView value, f32& out) -> bool {
            const auto trimmed = TrimView(value);
            if (trimmed.IsEmpty()) {
                return false;
            }
            std::basic_string<TChar> temp(trimmed.Data(), trimmed.Data() + trimmed.Length());
            TChar*                   endPtr = nullptr;
            const double             parsed = ParseFloatLiteral(temp.c_str(), &endPtr);
            if (endPtr == temp.c_str() || *endPtr != static_cast<TChar>(0)) {
                return false;
            }
            out = static_cast<f32>(parsed);
            return true;
        }

        void AppendDiagnostic(FString& dst, FStringView msg) {
            if (msg.IsEmpty()) {
                return;
            }
            if (!dst.IsEmptyString()) {
                dst.Append(TEXT("\n"));
            }
            dst.Append(msg);
        }

        void AppendDiagnostic(FString& dst, const TChar* msg) {
            if ((msg == nullptr) || (msg[0] == static_cast<TChar>(0))) {
                return;
            }
            if (!dst.IsEmptyString()) {
                dst.Append(TEXT("\n"));
            }
            dst.Append(msg);
        }

        auto StartsWith(FStringView text, FStringView prefix) -> bool {
            return text.StartsWith(prefix);
        }

        auto StripInlineComment(FStringView line) -> FStringView {
            const auto lineComment  = line.Find(FStringView(TEXT("//")));
            const auto blockComment = line.Find(FStringView(TEXT("/*")));
            if (lineComment == FStringView::npos && blockComment == FStringView::npos) {
                return line;
            }
            auto cut = lineComment;
            if (cut == FStringView::npos
                || (blockComment != FStringView::npos && blockComment < cut)) {
                cut = blockComment;
            }
            return line.Substr(0, cut);
        }

        auto NormalizeAltinaBlock(FStringView block) -> FString {
            FString     output;
            usize       pos    = 0U;
            const usize length = block.Length();
            while (pos < length) {
                usize lineEnd = pos;
                while (lineEnd < length && block[lineEnd] != static_cast<TChar>('\n')
                    && block[lineEnd] != static_cast<TChar>('\r')) {
                    ++lineEnd;
                }
                auto line = TrimView(block.Substr(pos, lineEnd - pos));
                if (StartsWith(line, FStringView(TEXT("//")))) {
                    line = TrimView(line.Substr(2));
                } else if (StartsWith(line, FStringView(TEXT("*")))) {
                    line = TrimView(line.Substr(1));
                }
                line = StripInlineComment(line);
                line = TrimView(line);
                if (!line.IsEmpty()) {
                    if (!output.IsEmptyString()) {
                        output.Append(TEXT("\n"));
                    }
                    output.Append(line);
                }
                if (lineEnd < length && block[lineEnd] == static_cast<TChar>('\r')
                    && (lineEnd + 1U) < length && block[lineEnd + 1U] == static_cast<TChar>('\n')) {
                    pos = lineEnd + 2U;
                } else {
                    pos = lineEnd + 1U;
                }
            }
            return output;
        }
        struct FLineCursor {
            FStringView mLine;
            usize       mPos = 0U;

            auto        IsEnd() const -> bool { return mPos >= mLine.Length(); }

            auto        Peek() const -> TChar {
                if (IsEnd()) {
                    return static_cast<TChar>(0);
                }
                return mLine[mPos];
            }

            void Advance() {
                if (!IsEnd()) {
                    ++mPos;
                }
            }

            void SkipWhitespace() {
                while (!IsEnd() && IsWhitespace(Peek())) {
                    Advance();
                }
            }

            auto MatchChar(TChar ch) -> bool {
                SkipWhitespace();
                if (Peek() != ch) {
                    return false;
                }
                Advance();
                return true;
            }

            auto ParseIdentifier(FString& out) -> bool {
                SkipWhitespace();
                if (!IsIdentifierStart(Peek())) {
                    return false;
                }
                const usize start = mPos;
                Advance();
                while (!IsEnd() && IsIdentifierChar(Peek())) {
                    Advance();
                }
                out.Assign(mLine.Substr(start, mPos - start));
                return true;
            }

            auto ParseNumber(i32& out) -> bool {
                SkipWhitespace();
                bool negative = false;
                if (Peek() == static_cast<TChar>('+') || Peek() == static_cast<TChar>('-')) {
                    negative = (Peek() == static_cast<TChar>('-'));
                    Advance();
                }
                if (!IsDigit(Peek())) {
                    return false;
                }
                i64 value = 0;
                while (!IsEnd() && IsDigit(Peek())) {
                    value = value * 10 + static_cast<i64>(Peek() - static_cast<TChar>('0'));
                    Advance();
                }
                if (negative) {
                    value = -value;
                }
                out = static_cast<i32>(value);
                return true;
            }
        };

        auto FindDimensionIndex(const FShaderPermutationLayout& layout, FStringView name) -> i32 {
            for (usize i = 0; i < layout.mDimensions.Size(); ++i) {
                if (layout.mDimensions[i].mName == name) {
                    return static_cast<i32>(i);
                }
            }
            return -1;
        }

        auto FindBuiltinIndex(const FShaderBuiltinLayout& layout, FStringView name) -> i32 {
            for (usize i = 0; i < layout.mBuiltins.Size(); ++i) {
                if (layout.mBuiltins[i].mName == name) {
                    return static_cast<i32>(i);
                }
            }
            return -1;
        }

        auto ParsePermutationLine(
            FStringView line, FShaderPermutationDimension& out, FString& diagnostics) -> bool {
            FLineCursor cursor{ line };
            FString     name;
            if (!cursor.ParseIdentifier(name)) {
                AppendDiagnostic(diagnostics, TEXT("Permutation entry missing identifier."));
                return false;
            }
            if (!cursor.MatchChar(static_cast<TChar>(':'))) {
                AppendDiagnostic(diagnostics, TEXT("Permutation entry missing ':' separator."));
                return false;
            }

            FString typeName;
            if (!cursor.ParseIdentifier(typeName)) {
                AppendDiagnostic(diagnostics, TEXT("Permutation entry missing type."));
                return false;
            }

            out       = {};
            out.mName = name;
            if (typeName == FString(TEXT("bool"))) {
                out.mType         = EShaderPermutationValueType::Bool;
                out.mMinValue     = 0;
                out.mMaxValue     = 1;
                out.mDefaultValue = 0;
            } else if (typeName == FString(TEXT("int"))) {
                out.mType = EShaderPermutationValueType::Int;
            } else if (typeName == FString(TEXT("enum"))) {
                out.mType = EShaderPermutationValueType::Enum;
                if (!cursor.MatchChar(static_cast<TChar>('{'))) {
                    AppendDiagnostic(diagnostics, TEXT("Enum permutation missing '{'."));
                    return false;
                }
                while (true) {
                    i32 value = 0;
                    if (!cursor.ParseNumber(value)) {
                        AppendDiagnostic(diagnostics, TEXT("Enum permutation expects number."));
                        return false;
                    }
                    out.mEnumValues.PushBack(value);
                    cursor.SkipWhitespace();
                    if (cursor.MatchChar(static_cast<TChar>('}'))) {
                        break;
                    }
                    if (!cursor.MatchChar(static_cast<TChar>(','))) {
                        AppendDiagnostic(
                            diagnostics, TEXT("Enum permutation missing ',' separator."));
                        return false;
                    }
                }
                if (out.mEnumValues.IsEmpty()) {
                    AppendDiagnostic(diagnostics, TEXT("Enum permutation has no values."));
                    return false;
                }
                out.mDefaultValue = out.mEnumValues[0];
            } else {
                AppendDiagnostic(diagnostics, TEXT("Unknown permutation type."));
                return false;
            }

            bool hasDefault = false;
            bool hasRange   = false;
            while (!cursor.IsEnd()) {
                cursor.SkipWhitespace();
                if (cursor.IsEnd()) {
                    break;
                }
                const auto ch = cursor.Peek();
                if (ch == static_cast<TChar>('=')) {
                    cursor.Advance();
                    i32 value = 0;
                    if (!cursor.ParseNumber(value)) {
                        AppendDiagnostic(diagnostics, TEXT("Default permutation value invalid."));
                        return false;
                    }
                    out.mDefaultValue = value;
                    hasDefault        = true;
                } else if (ch == static_cast<TChar>('[')) {
                    cursor.Advance();
                    cursor.SkipWhitespace();
                    if (IsDigit(cursor.Peek()) || cursor.Peek() == static_cast<TChar>('-')) {
                        i32 minValue = 0;
                        if (!cursor.ParseNumber(minValue)) {
                            AppendDiagnostic(
                                diagnostics, TEXT("Permutation range missing min value."));
                            return false;
                        }
                        cursor.SkipWhitespace();
                        if (!cursor.MatchChar(static_cast<TChar>('.'))
                            || !cursor.MatchChar(static_cast<TChar>('.'))) {
                            AppendDiagnostic(diagnostics, TEXT("Permutation range missing '..'."));
                            return false;
                        }
                        i32 maxValue = 0;
                        if (!cursor.ParseNumber(maxValue)) {
                            AppendDiagnostic(
                                diagnostics, TEXT("Permutation range missing max value."));
                            return false;
                        }
                        cursor.SkipWhitespace();
                        if (!cursor.MatchChar(static_cast<TChar>(']'))) {
                            AppendDiagnostic(diagnostics, TEXT("Permutation range missing ']'."));
                            return false;
                        }
                        out.mMinValue = minValue;
                        out.mMaxValue = maxValue;
                        hasRange      = true;
                    } else {
                        FString tag;
                        if (!cursor.ParseIdentifier(tag)) {
                            AppendDiagnostic(diagnostics, TEXT("Permutation domain tag missing."));
                            return false;
                        }
                        cursor.SkipWhitespace();
                        if (!cursor.MatchChar(static_cast<TChar>(']'))) {
                            AppendDiagnostic(
                                diagnostics, TEXT("Permutation domain tag missing ']'."));
                            return false;
                        }
                        if (tag == FString(TEXT("multi"))) {
                            out.mDomain = EShaderPermutationDomain::Multi;
                        } else if (tag == FString(TEXT("feature"))) {
                            out.mDomain = EShaderPermutationDomain::Feature;
                        } else {
                            AppendDiagnostic(diagnostics, TEXT("Unknown permutation domain tag."));
                            return false;
                        }
                    }
                } else if (ch == static_cast<TChar>(';') || ch == static_cast<TChar>(',')) {
                    cursor.Advance();
                } else {
                    cursor.Advance();
                }
            }

            if (out.mType == EShaderPermutationValueType::Int) {
                if (!hasRange) {
                    AppendDiagnostic(diagnostics,
                        TEXT("Integer permutation requires explicit [min..max] range."));
                    return false;
                }
                if (!hasDefault) {
                    out.mDefaultValue = out.mMinValue;
                }
                if (out.mDefaultValue < out.mMinValue || out.mDefaultValue > out.mMaxValue) {
                    AppendDiagnostic(
                        diagnostics, TEXT("Integer permutation default out of range."));
                    return false;
                }
            } else if (out.mType == EShaderPermutationValueType::Enum) {
                if (hasDefault) {
                    bool found = false;
                    for (const auto value : out.mEnumValues) {
                        if (value == out.mDefaultValue) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        AppendDiagnostic(diagnostics,
                            TEXT("Enum permutation default must be one of enum values."));
                        return false;
                    }
                }
            } else if (out.mType == EShaderPermutationValueType::Bool) {
                if (out.mDefaultValue != 0 && out.mDefaultValue != 1) {
                    AppendDiagnostic(diagnostics, TEXT("Bool permutation default must be 0 or 1."));
                    return false;
                }
            }

            return true;
        }

        auto ParseBuiltinLine(FStringView line, FShaderBuiltinDefinition& out, FString& diagnostics)
            -> bool {
            FLineCursor cursor{ line };
            FString     name;
            if (!cursor.ParseIdentifier(name)) {
                AppendDiagnostic(diagnostics, TEXT("Builtin entry missing identifier."));
                return false;
            }
            if (!cursor.MatchChar(static_cast<TChar>(':'))) {
                AppendDiagnostic(diagnostics, TEXT("Builtin entry missing ':' separator."));
                return false;
            }
            FString typeName;
            if (!cursor.ParseIdentifier(typeName)) {
                AppendDiagnostic(diagnostics, TEXT("Builtin entry missing type."));
                return false;
            }

            out       = {};
            out.mName = name;
            if (!out.mName.StartsWith(FStringView(TEXT("AE_BUILTIN_")))) {
                AppendDiagnostic(diagnostics, TEXT("Builtin name must start with AE_BUILTIN_."));
                return false;
            }

            if (typeName == FString(TEXT("bool"))) {
                out.mType         = EShaderPermutationValueType::Bool;
                out.mDefaultValue = 0;
            } else if (typeName == FString(TEXT("int"))) {
                out.mType         = EShaderPermutationValueType::Int;
                out.mDefaultValue = 0;
            } else {
                AppendDiagnostic(diagnostics, TEXT("Unsupported builtin type."));
                return false;
            }

            return true;
        }
        auto ParsePermutationBlock(const FString& normalized, FShaderPermutationLayout& layout,
            FString& diagnostics) -> bool {
            usize       pos    = 0U;
            const auto  view   = normalized.ToView();
            const usize length = view.Length();
            while (pos < length) {
                usize lineEnd = pos;
                while (lineEnd < length && view[lineEnd] != static_cast<TChar>('\n')
                    && view[lineEnd] != static_cast<TChar>('\r')) {
                    ++lineEnd;
                }
                auto line = TrimView(view.Substr(pos, lineEnd - pos));
                if (!line.IsEmpty()) {
                    FShaderPermutationDimension dim;
                    if (!ParsePermutationLine(line, dim, diagnostics)) {
                        return false;
                    }
                    if (FindDimensionIndex(layout, dim.mName) >= 0) {
                        AppendDiagnostic(diagnostics, TEXT("Duplicate permutation name."));
                        return false;
                    }
                    layout.mDimensions.PushBack(dim);
                }
                if (lineEnd < length && view[lineEnd] == static_cast<TChar>('\r')
                    && (lineEnd + 1U) < length && view[lineEnd + 1U] == static_cast<TChar>('\n')) {
                    pos = lineEnd + 2U;
                } else {
                    pos = lineEnd + 1U;
                }
            }
            return true;
        }

        auto ParseBuiltinsBlock(
            const FString& normalized, FShaderBuiltinLayout& layout, FString& diagnostics) -> bool {
            usize       pos    = 0U;
            const auto  view   = normalized.ToView();
            const usize length = view.Length();
            while (pos < length) {
                usize lineEnd = pos;
                while (lineEnd < length && view[lineEnd] != static_cast<TChar>('\n')
                    && view[lineEnd] != static_cast<TChar>('\r')) {
                    ++lineEnd;
                }
                auto line = TrimView(view.Substr(pos, lineEnd - pos));
                if (!line.IsEmpty()) {
                    FShaderBuiltinDefinition builtin;
                    if (!ParseBuiltinLine(line, builtin, diagnostics)) {
                        return false;
                    }
                    if (FindBuiltinIndex(layout, builtin.mName) >= 0) {
                        AppendDiagnostic(diagnostics, TEXT("Duplicate builtin name."));
                        return false;
                    }
                    layout.mBuiltins.PushBack(builtin);
                }
                if (lineEnd < length && view[lineEnd] == static_cast<TChar>('\r')
                    && (lineEnd + 1U) < length && view[lineEnd + 1U] == static_cast<TChar>('\n')) {
                    pos = lineEnd + 2U;
                } else {
                    pos = lineEnd + 1U;
                }
            }
            return true;
        }

        auto ParseRasterStateBlock(
            const FString& normalized, FShaderRasterState& state, FString& diagnostics) -> bool {
            state              = {};
            usize       pos    = 0U;
            const auto  view   = normalized.ToView();
            const usize length = view.Length();
            while (pos < length) {
                usize lineEnd = pos;
                while (lineEnd < length && view[lineEnd] != static_cast<TChar>('\n')
                    && view[lineEnd] != static_cast<TChar>('\r')) {
                    ++lineEnd;
                }
                auto line = TrimLineTerminator(view.Substr(pos, lineEnd - pos));
                if (!line.IsEmpty()) {
                    const auto eqPos = line.Find(FStringView(TEXT("=")));
                    if (eqPos == FStringView::npos) {
                        AppendDiagnostic(diagnostics, TEXT("Raster state line missing '='."));
                        return false;
                    }
                    const auto key = TrimView(line.Substr(0, eqPos));
                    auto       value =
                        TrimLineTerminator(line.Substr(eqPos + 1U, line.Length() - eqPos - 1U));
                    if (key.IsEmpty() || value.IsEmpty()) {
                        AppendDiagnostic(
                            diagnostics, TEXT("Raster state line requires key and value."));
                        return false;
                    }

                    if (key == FStringView(TEXT("fill")) || key == FStringView(TEXT("fill_mode"))) {
                        if (value == FStringView(TEXT("solid"))) {
                            state.mFillMode = Shader::EShaderRasterFillMode::Solid;
                        } else if (value == FStringView(TEXT("wireframe"))) {
                            state.mFillMode = Shader::EShaderRasterFillMode::Wireframe;
                        } else {
                            AppendDiagnostic(diagnostics, TEXT("Invalid fill mode."));
                            return false;
                        }
                    } else if (key == FStringView(TEXT("cull"))
                        || key == FStringView(TEXT("cull_mode"))) {
                        if (value == FStringView(TEXT("none"))) {
                            state.mCullMode = Shader::EShaderRasterCullMode::None;
                        } else if (value == FStringView(TEXT("front"))) {
                            state.mCullMode = Shader::EShaderRasterCullMode::Front;
                        } else if (value == FStringView(TEXT("back"))) {
                            state.mCullMode = Shader::EShaderRasterCullMode::Back;
                        } else {
                            AppendDiagnostic(diagnostics, TEXT("Invalid cull mode."));
                            return false;
                        }
                    } else if (key == FStringView(TEXT("front_face"))
                        || key == FStringView(TEXT("frontface"))) {
                        if (value == FStringView(TEXT("ccw"))) {
                            state.mFrontFace = Shader::EShaderRasterFrontFace::CCW;
                        } else if (value == FStringView(TEXT("cw"))) {
                            state.mFrontFace = Shader::EShaderRasterFrontFace::CW;
                        } else {
                            AppendDiagnostic(diagnostics, TEXT("Invalid front face."));
                            return false;
                        }
                    } else if (key == FStringView(TEXT("depth_bias"))) {
                        i32 bias = 0;
                        if (!ParseIntValue(value, bias)) {
                            AppendDiagnostic(diagnostics, TEXT("Invalid depth bias."));
                            return false;
                        }
                        state.mDepthBias = bias;
                    } else if (key == FStringView(TEXT("depth_bias_clamp"))) {
                        f32 clamp = 0.0f;
                        if (!ParseFloatValue(value, clamp)) {
                            AppendDiagnostic(diagnostics, TEXT("Invalid depth bias clamp."));
                            return false;
                        }
                        state.mDepthBiasClamp = clamp;
                    } else if (key == FStringView(TEXT("slope_scaled_depth_bias"))
                        || key == FStringView(TEXT("slope_depth_bias"))) {
                        f32 slope = 0.0f;
                        if (!ParseFloatValue(value, slope)) {
                            AppendDiagnostic(diagnostics, TEXT("Invalid slope scaled depth bias."));
                            return false;
                        }
                        state.mSlopeScaledDepthBias = slope;
                    } else if (key == FStringView(TEXT("depth_clip"))) {
                        bool enabled = true;
                        if (!ParseBoolValue(value, enabled)) {
                            AppendDiagnostic(diagnostics, TEXT("Invalid depth clip value."));
                            return false;
                        }
                        state.mDepthClip = enabled;
                    } else if (key == FStringView(TEXT("conservative"))
                        || key == FStringView(TEXT("conservative_raster"))) {
                        bool enabled = false;
                        if (!ParseBoolValue(value, enabled)) {
                            AppendDiagnostic(
                                diagnostics, TEXT("Invalid conservative raster value."));
                            return false;
                        }
                        state.mConservativeRaster = enabled;
                    } else {
                        AppendDiagnostic(diagnostics, TEXT("Unknown raster state key."));
                        return false;
                    }
                }
                if (lineEnd < length && view[lineEnd] == static_cast<TChar>('\r')
                    && (lineEnd + 1U) < length && view[lineEnd + 1U] == static_cast<TChar>('\n')) {
                    pos = lineEnd + 2U;
                } else {
                    pos = lineEnd + 1U;
                }
            }
            return true;
        }

        struct FRuleToken {
            enum class EKind : u8 {
                Identifier,
                Number,
                Symbol,
                End
            };
            EKind       mKind = EKind::End;
            FStringView mText;
            i32         mNumber = 0;
        };

        class FRuleLexer {
        public:
            explicit FRuleLexer(FStringView text) : mText(text), mPos(0U) {}

            auto Next() -> FRuleToken {
                SkipWhitespaceAndComments();
                if (mPos >= mText.Length()) {
                    return { FRuleToken::EKind::End, {}, 0 };
                }
                const auto ch = mText[mPos];
                if (IsIdentifierStart(ch)) {
                    const usize start = mPos++;
                    while (mPos < mText.Length() && IsIdentifierChar(mText[mPos])) {
                        ++mPos;
                    }
                    return { FRuleToken::EKind::Identifier, mText.Substr(start, mPos - start), 0 };
                }
                if (IsDigit(ch)) {
                    const usize start = mPos;
                    i64         value = 0;
                    while (mPos < mText.Length() && IsDigit(mText[mPos])) {
                        value =
                            value * 10 + static_cast<i64>(mText[mPos] - static_cast<TChar>('0'));
                        ++mPos;
                    }
                    return { FRuleToken::EKind::Number, mText.Substr(start, mPos - start),
                        static_cast<i32>(value) };
                }

                auto MakeSymbol = [&](usize length) -> FRuleToken {
                    const auto start = mPos;
                    mPos += length;
                    return { FRuleToken::EKind::Symbol, mText.Substr(start, length), 0 };
                };

                if ((ch == static_cast<TChar>('&')) && (mPos + 1U < mText.Length())
                    && (mText[mPos + 1U] == static_cast<TChar>('&'))) {
                    return MakeSymbol(2);
                }
                if ((ch == static_cast<TChar>('|')) && (mPos + 1U < mText.Length())
                    && (mText[mPos + 1U] == static_cast<TChar>('|'))) {
                    return MakeSymbol(2);
                }
                if ((ch == static_cast<TChar>('=')) && (mPos + 1U < mText.Length())
                    && (mText[mPos + 1U] == static_cast<TChar>('='))) {
                    return MakeSymbol(2);
                }
                if ((ch == static_cast<TChar>('!')) && (mPos + 1U < mText.Length())
                    && (mText[mPos + 1U] == static_cast<TChar>('='))) {
                    return MakeSymbol(2);
                }
                if ((ch == static_cast<TChar>('<')) && (mPos + 1U < mText.Length())
                    && (mText[mPos + 1U] == static_cast<TChar>('='))) {
                    return MakeSymbol(2);
                }
                if ((ch == static_cast<TChar>('>')) && (mPos + 1U < mText.Length())
                    && (mText[mPos + 1U] == static_cast<TChar>('='))) {
                    return MakeSymbol(2);
                }
                if ((ch == static_cast<TChar>('.')) && (mPos + 1U < mText.Length())
                    && (mText[mPos + 1U] == static_cast<TChar>('.'))) {
                    return MakeSymbol(2);
                }
                return MakeSymbol(1);
            }

        private:
            void SkipWhitespaceAndComments() {
                while (mPos < mText.Length()) {
                    const auto ch = mText[mPos];
                    if (IsWhitespace(ch)) {
                        ++mPos;
                        continue;
                    }
                    if (ch == static_cast<TChar>('/') && (mPos + 1U) < mText.Length()) {
                        const auto next = mText[mPos + 1U];
                        if (next == static_cast<TChar>('/')) {
                            mPos += 2U;
                            while (mPos < mText.Length() && mText[mPos] != static_cast<TChar>('\n')
                                && mText[mPos] != static_cast<TChar>('\r')) {
                                ++mPos;
                            }
                            continue;
                        }
                        if (next == static_cast<TChar>('*')) {
                            mPos += 2U;
                            while ((mPos + 1U) < mText.Length()) {
                                if (mText[mPos] == static_cast<TChar>('*')
                                    && mText[mPos + 1U] == static_cast<TChar>('/')) {
                                    mPos += 2U;
                                    break;
                                }
                                ++mPos;
                            }
                            continue;
                        }
                    }
                    break;
                }
            }

            FStringView mText;
            usize       mPos = 0U;
        };
        class FRuleParser {
        public:
            FRuleParser(FStringView text, const FShaderPermutationLayout& layout,
                const FShaderBuiltinLayout& builtins)
                : mLexer(text), mLayout(layout), mBuiltins(builtins) {
                Advance();
            }

            auto Parse(FShaderPermutationRuleSet& outRules, FString& diagnostics) -> bool {
                while (mCurrent.mKind != FRuleToken::EKind::End) {
                    if (MatchIdentifier(TEXT("let"))) {
                        if (!ParseLet(outRules, diagnostics)) {
                            return false;
                        }
                    } else if (MatchIdentifier(TEXT("require"))) {
                        if (!ParseRequire(outRules, diagnostics)) {
                            return false;
                        }
                    } else {
                        AppendDiagnostic(diagnostics, TEXT("Unknown rule statement."));
                        return false;
                    }
                }
                return true;
            }

        private:
            auto MatchSymbol(FStringView symbol) -> bool {
                if (mCurrent.mKind != FRuleToken::EKind::Symbol || mCurrent.mText != symbol) {
                    return false;
                }
                Advance();
                return true;
            }

            auto MatchIdentifier(FStringView ident) -> bool {
                if (mCurrent.mKind != FRuleToken::EKind::Identifier || mCurrent.mText != ident) {
                    return false;
                }
                Advance();
                return true;
            }

            auto ExpectIdentifier(FString& out, FString& diagnostics) -> bool {
                if (mCurrent.mKind != FRuleToken::EKind::Identifier) {
                    AppendDiagnostic(diagnostics, TEXT("Expected identifier."));
                    return false;
                }
                out.Assign(mCurrent.mText);
                Advance();
                return true;
            }

            auto ParseLet(FShaderPermutationRuleSet& outRules, FString& diagnostics) -> bool {
                FString name;
                if (!ExpectIdentifier(name, diagnostics)) {
                    return false;
                }
                if (FindDimensionIndex(mLayout, name) >= 0 || FindBuiltinIndex(mBuiltins, name) >= 0
                    || HasLet(outRules, name)) {
                    AppendDiagnostic(diagnostics, TEXT("Let name conflicts with existing symbol."));
                    return false;
                }
                if (!MatchSymbol(FStringView(TEXT("=")))) {
                    AppendDiagnostic(diagnostics, TEXT("Let missing '='."));
                    return false;
                }
                const auto exprIndex = ParseExpression(outRules, diagnostics);
                if (exprIndex < 0) {
                    return false;
                }
                if (MatchSymbol(FStringView(TEXT(";")))) {}
                outRules.mLets.PushBack({ name, exprIndex });
                return true;
            }

            auto ParseRequire(FShaderPermutationRuleSet& outRules, FString& diagnostics) -> bool {
                const auto exprIndex = ParseExpression(outRules, diagnostics);
                if (exprIndex < 0) {
                    return false;
                }
                if (MatchSymbol(FStringView(TEXT(";")))) {}
                outRules.mRequires.PushBack(exprIndex);
                return true;
            }

            auto HasLet(const FShaderPermutationRuleSet& rules, FStringView name) -> bool {
                for (const auto& letRule : rules.mLets) {
                    if (letRule.mName == name) {
                        return true;
                    }
                }
                return false;
            }

            auto ParseExpression(FShaderPermutationRuleSet& outRules, FString& diagnostics) -> i32 {
                return ParseLogicalOr(outRules, diagnostics);
            }

            auto ParseLogicalOr(FShaderPermutationRuleSet& outRules, FString& diagnostics) -> i32 {
                auto lhs = ParseLogicalAnd(outRules, diagnostics);
                if (lhs < 0) {
                    return -1;
                }
                while (MatchSymbol(FStringView(TEXT("||")))) {
                    auto rhs = ParseLogicalAnd(outRules, diagnostics);
                    if (rhs < 0) {
                        return -1;
                    }
                    lhs = AddBinary(outRules, EShaderPermutationRuleOperator::LogicalOr, lhs, rhs);
                }
                return lhs;
            }

            auto ParseLogicalAnd(FShaderPermutationRuleSet& outRules, FString& diagnostics) -> i32 {
                auto lhs = ParseEquality(outRules, diagnostics);
                if (lhs < 0) {
                    return -1;
                }
                while (MatchSymbol(FStringView(TEXT("&&")))) {
                    auto rhs = ParseEquality(outRules, diagnostics);
                    if (rhs < 0) {
                        return -1;
                    }
                    lhs = AddBinary(outRules, EShaderPermutationRuleOperator::LogicalAnd, lhs, rhs);
                }
                return lhs;
            }

            auto ParseEquality(FShaderPermutationRuleSet& outRules, FString& diagnostics) -> i32 {
                auto lhs = ParseRelational(outRules, diagnostics);
                if (lhs < 0) {
                    return -1;
                }
                while (true) {
                    if (MatchSymbol(FStringView(TEXT("==")))) {
                        auto rhs = ParseRelational(outRules, diagnostics);
                        if (rhs < 0) {
                            return -1;
                        }
                        lhs = AddBinary(outRules, EShaderPermutationRuleOperator::Equal, lhs, rhs);
                        continue;
                    }
                    if (MatchSymbol(FStringView(TEXT("!=")))) {
                        auto rhs = ParseRelational(outRules, diagnostics);
                        if (rhs < 0) {
                            return -1;
                        }
                        lhs =
                            AddBinary(outRules, EShaderPermutationRuleOperator::NotEqual, lhs, rhs);
                        continue;
                    }
                    break;
                }
                return lhs;
            }

            auto ParseRelational(FShaderPermutationRuleSet& outRules, FString& diagnostics) -> i32 {
                auto lhs = ParseAdditive(outRules, diagnostics);
                if (lhs < 0) {
                    return -1;
                }
                while (true) {
                    if (MatchSymbol(FStringView(TEXT("<=")))) {
                        auto rhs = ParseAdditive(outRules, diagnostics);
                        if (rhs < 0) {
                            return -1;
                        }
                        lhs = AddBinary(
                            outRules, EShaderPermutationRuleOperator::LessEqual, lhs, rhs);
                        continue;
                    }
                    if (MatchSymbol(FStringView(TEXT(">=")))) {
                        auto rhs = ParseAdditive(outRules, diagnostics);
                        if (rhs < 0) {
                            return -1;
                        }
                        lhs = AddBinary(
                            outRules, EShaderPermutationRuleOperator::GreaterEqual, lhs, rhs);
                        continue;
                    }
                    if (MatchSymbol(FStringView(TEXT("<")))) {
                        auto rhs = ParseAdditive(outRules, diagnostics);
                        if (rhs < 0) {
                            return -1;
                        }
                        lhs = AddBinary(outRules, EShaderPermutationRuleOperator::Less, lhs, rhs);
                        continue;
                    }
                    if (MatchSymbol(FStringView(TEXT(">")))) {
                        auto rhs = ParseAdditive(outRules, diagnostics);
                        if (rhs < 0) {
                            return -1;
                        }
                        lhs =
                            AddBinary(outRules, EShaderPermutationRuleOperator::Greater, lhs, rhs);
                        continue;
                    }
                    break;
                }
                return lhs;
            }

            auto ParseAdditive(FShaderPermutationRuleSet& outRules, FString& diagnostics) -> i32 {
                auto lhs = ParseMultiplicative(outRules, diagnostics);
                if (lhs < 0) {
                    return -1;
                }
                while (true) {
                    if (MatchSymbol(FStringView(TEXT("+")))) {
                        auto rhs = ParseMultiplicative(outRules, diagnostics);
                        if (rhs < 0) {
                            return -1;
                        }
                        lhs = AddBinary(outRules, EShaderPermutationRuleOperator::Add, lhs, rhs);
                        continue;
                    }
                    if (MatchSymbol(FStringView(TEXT("-")))) {
                        auto rhs = ParseMultiplicative(outRules, diagnostics);
                        if (rhs < 0) {
                            return -1;
                        }
                        lhs =
                            AddBinary(outRules, EShaderPermutationRuleOperator::Subtract, lhs, rhs);
                        continue;
                    }
                    break;
                }
                return lhs;
            }

            auto ParseMultiplicative(FShaderPermutationRuleSet& outRules, FString& diagnostics)
                -> i32 {
                auto lhs = ParseUnary(outRules, diagnostics);
                if (lhs < 0) {
                    return -1;
                }
                while (true) {
                    if (MatchSymbol(FStringView(TEXT("*")))) {
                        auto rhs = ParseUnary(outRules, diagnostics);
                        if (rhs < 0) {
                            return -1;
                        }
                        lhs =
                            AddBinary(outRules, EShaderPermutationRuleOperator::Multiply, lhs, rhs);
                        continue;
                    }
                    if (MatchSymbol(FStringView(TEXT("/")))) {
                        auto rhs = ParseUnary(outRules, diagnostics);
                        if (rhs < 0) {
                            return -1;
                        }
                        lhs = AddBinary(outRules, EShaderPermutationRuleOperator::Divide, lhs, rhs);
                        continue;
                    }
                    if (MatchSymbol(FStringView(TEXT("%")))) {
                        auto rhs = ParseUnary(outRules, diagnostics);
                        if (rhs < 0) {
                            return -1;
                        }
                        lhs = AddBinary(outRules, EShaderPermutationRuleOperator::Modulo, lhs, rhs);
                        continue;
                    }
                    break;
                }
                return lhs;
            }

            auto ParseUnary(FShaderPermutationRuleSet& outRules, FString& diagnostics) -> i32 {
                if (MatchSymbol(FStringView(TEXT("!")))) {
                    auto operand = ParseUnary(outRules, diagnostics);
                    if (operand < 0) {
                        return -1;
                    }
                    return AddUnary(outRules, EShaderPermutationRuleOperator::LogicalNot, operand);
                }
                if (MatchSymbol(FStringView(TEXT("-")))) {
                    auto operand = ParseUnary(outRules, diagnostics);
                    if (operand < 0) {
                        return -1;
                    }
                    return AddUnary(outRules, EShaderPermutationRuleOperator::UnaryNegate, operand);
                }
                if (MatchSymbol(FStringView(TEXT("+")))) {
                    auto operand = ParseUnary(outRules, diagnostics);
                    if (operand < 0) {
                        return -1;
                    }
                    return AddUnary(outRules, EShaderPermutationRuleOperator::UnaryPlus, operand);
                }
                return ParsePrimary(outRules, diagnostics);
            }

            auto ParsePrimary(FShaderPermutationRuleSet& outRules, FString& diagnostics) -> i32 {
                if (MatchSymbol(FStringView(TEXT("(")))) {
                    const auto expr = ParseExpression(outRules, diagnostics);
                    if (expr < 0) {
                        return -1;
                    }
                    if (!MatchSymbol(FStringView(TEXT(")")))) {
                        AppendDiagnostic(diagnostics, TEXT("Missing ')' in expression."));
                        return -1;
                    }
                    return expr;
                }
                if (mCurrent.mKind == FRuleToken::EKind::Number) {
                    const auto value = mCurrent.mNumber;
                    Advance();
                    return AddLiteral(outRules, value);
                }
                if (mCurrent.mKind == FRuleToken::EKind::Identifier) {
                    FString name;
                    name.Assign(mCurrent.mText);
                    Advance();
                    return AddIdentifier(outRules, name);
                }
                AppendDiagnostic(diagnostics, TEXT("Unexpected token in expression."));
                return -1;
            }

            auto AddLiteral(FShaderPermutationRuleSet& outRules, i32 value) -> i32 {
                const auto index = static_cast<i32>(outRules.mExpressions.Size());
                auto&      node  = outRules.mExpressions.EmplaceBack();
                node.mKind       = FShaderPermutationRuleExpression::EKind::Literal;
                node.mLiteral    = value;
                return index;
            }

            auto AddIdentifier(FShaderPermutationRuleSet& outRules, const FString& name) -> i32 {
                const auto index = static_cast<i32>(outRules.mExpressions.Size());
                auto&      node  = outRules.mExpressions.EmplaceBack();
                node.mKind       = FShaderPermutationRuleExpression::EKind::Identifier;
                node.mIdentifier.Assign(name);
                return index;
            }

            auto AddUnary(FShaderPermutationRuleSet& outRules, EShaderPermutationRuleOperator op,
                i32 operand) -> i32 {
                const auto index = static_cast<i32>(outRules.mExpressions.Size());
                auto&      node  = outRules.mExpressions.EmplaceBack();
                node.mKind       = FShaderPermutationRuleExpression::EKind::Unary;
                node.mOperator   = op;
                node.mLeftIndex  = operand;
                return index;
            }

            auto AddBinary(FShaderPermutationRuleSet& outRules, EShaderPermutationRuleOperator op,
                i32 left, i32 right) -> i32 {
                const auto index = static_cast<i32>(outRules.mExpressions.Size());
                auto&      node  = outRules.mExpressions.EmplaceBack();
                node.mKind       = FShaderPermutationRuleExpression::EKind::Binary;
                node.mOperator   = op;
                node.mLeftIndex  = left;
                node.mRightIndex = right;
                return index;
            }

            void                            Advance() { mCurrent = mLexer.Next(); }

            FRuleLexer                      mLexer;
            FRuleToken                      mCurrent;
            const FShaderPermutationLayout& mLayout;
            const FShaderBuiltinLayout&     mBuiltins;
        };

        auto ParseRulesBlock(const FString& normalized, const FShaderPermutationLayout& layout,
            const FShaderBuiltinLayout& builtins, FShaderPermutationRuleSet& rules,
            FString& diagnostics) -> bool {
            if (normalized.IsEmptyString()) {
                return true;
            }
            FRuleParser parser(normalized.ToView(), layout, builtins);
            return parser.Parse(rules, diagnostics);
        }

        auto IsValueAllowed(const FShaderPermutationDimension& dim, i32 value) -> bool {
            switch (dim.mType) {
                case EShaderPermutationValueType::Bool:
                    return value == 0 || value == 1;
                case EShaderPermutationValueType::Enum:
                    for (const auto v : dim.mEnumValues) {
                        if (v == value) {
                            return true;
                        }
                    }
                    return false;
                case EShaderPermutationValueType::Int:
                    return value >= dim.mMinValue && value <= dim.mMaxValue;
                default:
                    return false;
            }
        }

        auto IsBuiltinAllowed(const FShaderBuiltinDefinition& builtin, i32 value) -> bool {
            if (builtin.mType == EShaderPermutationValueType::Bool) {
                return value == 0 || value == 1;
            }
            return true;
        }

        auto EvaluateExpressionNode(const FShaderPermutationRuleSet& rules, i32 nodeIndex,
            const THashMap<FString, i32>& env, i32& outValue, FString* diagnostics) -> bool {
            if (nodeIndex < 0 || nodeIndex >= static_cast<i32>(rules.mExpressions.Size())) {
                if (diagnostics) {
                    AppendDiagnostic(*diagnostics, TEXT("Invalid expression node index."));
                }
                return false;
            }
            const auto& node = rules.mExpressions[static_cast<usize>(nodeIndex)];
            switch (node.mKind) {
                case FShaderPermutationRuleExpression::EKind::Literal:
                    outValue = node.mLiteral;
                    return true;
                case FShaderPermutationRuleExpression::EKind::Identifier:
                {
                    const auto it = env.find(node.mIdentifier);
                    if (it == env.end()) {
                        if (diagnostics) {
                            AppendDiagnostic(*diagnostics, TEXT("Unknown identifier in rule."));
                        }
                        return false;
                    }
                    outValue = it->second;
                    return true;
                }
                case FShaderPermutationRuleExpression::EKind::Unary:
                {
                    i32 operand = 0;
                    if (!EvaluateExpressionNode(
                            rules, node.mLeftIndex, env, operand, diagnostics)) {
                        return false;
                    }
                    switch (node.mOperator) {
                        case EShaderPermutationRuleOperator::LogicalNot:
                            outValue = (operand == 0) ? 1 : 0;
                            return true;
                        case EShaderPermutationRuleOperator::UnaryNegate:
                            outValue = -operand;
                            return true;
                        case EShaderPermutationRuleOperator::UnaryPlus:
                            outValue = operand;
                            return true;
                        default:
                            if (diagnostics) {
                                AppendDiagnostic(
                                    *diagnostics, TEXT("Unsupported unary operator in rule."));
                            }
                            return false;
                    }
                }
                case FShaderPermutationRuleExpression::EKind::Binary:
                {
                    i32 lhs = 0;
                    i32 rhs = 0;
                    if (!EvaluateExpressionNode(rules, node.mLeftIndex, env, lhs, diagnostics)
                        || !EvaluateExpressionNode(
                            rules, node.mRightIndex, env, rhs, diagnostics)) {
                        return false;
                    }
                    switch (node.mOperator) {
                        case EShaderPermutationRuleOperator::Add:
                            outValue = lhs + rhs;
                            return true;
                        case EShaderPermutationRuleOperator::Subtract:
                            outValue = lhs - rhs;
                            return true;
                        case EShaderPermutationRuleOperator::Multiply:
                            outValue = lhs * rhs;
                            return true;
                        case EShaderPermutationRuleOperator::Divide:
                            if (rhs == 0) {
                                if (diagnostics) {
                                    AppendDiagnostic(
                                        *diagnostics, TEXT("Division by zero in rule."));
                                }
                                return false;
                            }
                            outValue = lhs / rhs;
                            return true;
                        case EShaderPermutationRuleOperator::Modulo:
                            if (rhs == 0) {
                                if (diagnostics) {
                                    AppendDiagnostic(*diagnostics, TEXT("Modulo by zero in rule."));
                                }
                                return false;
                            }
                            outValue = lhs % rhs;
                            return true;
                        case EShaderPermutationRuleOperator::Equal:
                            outValue = (lhs == rhs) ? 1 : 0;
                            return true;
                        case EShaderPermutationRuleOperator::NotEqual:
                            outValue = (lhs != rhs) ? 1 : 0;
                            return true;
                        case EShaderPermutationRuleOperator::Less:
                            outValue = (lhs < rhs) ? 1 : 0;
                            return true;
                        case EShaderPermutationRuleOperator::LessEqual:
                            outValue = (lhs <= rhs) ? 1 : 0;
                            return true;
                        case EShaderPermutationRuleOperator::Greater:
                            outValue = (lhs > rhs) ? 1 : 0;
                            return true;
                        case EShaderPermutationRuleOperator::GreaterEqual:
                            outValue = (lhs >= rhs) ? 1 : 0;
                            return true;
                        case EShaderPermutationRuleOperator::LogicalAnd:
                            outValue = (lhs != 0 && rhs != 0) ? 1 : 0;
                            return true;
                        case EShaderPermutationRuleOperator::LogicalOr:
                            outValue = (lhs != 0 || rhs != 0) ? 1 : 0;
                            return true;
                        default:
                            if (diagnostics) {
                                AppendDiagnostic(
                                    *diagnostics, TEXT("Unsupported binary operator in rule."));
                            }
                            return false;
                    }
                }
                default:
                    if (diagnostics) {
                        AppendDiagnostic(*diagnostics, TEXT("Unknown expression kind."));
                    }
                    return false;
            }
        }

        auto GetDimensionValues(const FShaderPermutationDimension& dim, TVector<i32>& outValues)
            -> bool {
            outValues.Clear();
            switch (dim.mType) {
                case EShaderPermutationValueType::Bool:
                    outValues.PushBack(0);
                    outValues.PushBack(1);
                    return true;
                case EShaderPermutationValueType::Enum:
                    if (dim.mEnumValues.IsEmpty()) {
                        return false;
                    }
                    outValues = dim.mEnumValues;
                    return true;
                case EShaderPermutationValueType::Int:
                    if (dim.mMinValue > dim.mMaxValue) {
                        return false;
                    }
                    for (i32 value = dim.mMinValue; value <= dim.mMaxValue; ++value) {
                        outValues.PushBack(value);
                    }
                    return true;
                default:
                    return false;
            }
        }
    } // namespace
    auto ParseShaderPermutationSource(FStringView source, FShaderPermutationParseResult& out)
        -> bool {
        out = {};

        FString permBlock;
        FString builtinBlock;
        FString rulesBlock;
        FString rasterBlock;

        auto    ExtractBlock = [&](FStringView blockName, FString& outBlock) -> bool {
            const auto  marker = FStringView(TEXT("@altina"));
            usize       pos    = 0U;
            const usize length = source.Length();
            while (pos < length) {
                const auto found = source.Find(marker, pos);
                if (found == FStringView::npos) {
                    break;
                }
                usize scan = found + marker.Length();
                while (scan < length && IsWhitespace(source[scan])) {
                    ++scan;
                }
                const usize nameStart = scan;
                while (scan < length && IsIdentifierChar(source[scan])) {
                    ++scan;
                }
                if (nameStart == scan) {
                    pos = found + marker.Length();
                    continue;
                }
                const auto nameView = source.Substr(nameStart, scan - nameStart);
                if (nameView == blockName) {
                    while (scan < length && source[scan] != static_cast<TChar>('{')) {
                        ++scan;
                    }
                    if (scan >= length) {
                        return false;
                    }
                    const usize contentStart = scan + 1U;
                    int         depth        = 1;
                    ++scan;
                    while (scan < length && depth > 0) {
                        if (source[scan] == static_cast<TChar>('{')) {
                            ++depth;
                        } else if (source[scan] == static_cast<TChar>('}')) {
                            --depth;
                        }
                        ++scan;
                    }
                    if (depth != 0) {
                        return false;
                    }
                    const usize contentEnd = scan - 1U;
                    outBlock.Assign(source.Substr(contentStart, contentEnd - contentStart));
                    return true;
                }
                pos = found + marker.Length();
            }
            return false;
        };

        const bool hasPerm        = ExtractBlock(FStringView(TEXT("perm")), permBlock);
        const bool hasBuiltins    = ExtractBlock(FStringView(TEXT("builtins")), builtinBlock);
        const bool hasRules       = ExtractBlock(FStringView(TEXT("rules")), rulesBlock);
        const bool hasRasterState = ExtractBlock(FStringView(TEXT("raster_state")), rasterBlock);

        if (hasPerm) {
            const auto normalized = NormalizeAltinaBlock(permBlock.ToView());
            if (!ParsePermutationBlock(normalized, out.mPermutationLayout, out.mDiagnostics)) {
                out.mSucceeded = false;
                return false;
            }
        }

        if (hasBuiltins) {
            const auto normalized = NormalizeAltinaBlock(builtinBlock.ToView());
            if (!ParseBuiltinsBlock(normalized, out.mBuiltinLayout, out.mDiagnostics)) {
                out.mSucceeded = false;
                return false;
            }
        }

        if (hasRasterState) {
            const auto normalized = NormalizeAltinaBlock(rasterBlock.ToView());
            if (!ParseRasterStateBlock(normalized, out.mRasterState, out.mDiagnostics)) {
                out.mSucceeded = false;
                return false;
            }
            out.mHasRasterState = true;
        }

        if (hasRules) {
            const auto normalized = NormalizeAltinaBlock(rulesBlock.ToView());
            if (!ParseRulesBlock(normalized, out.mPermutationLayout, out.mBuiltinLayout, out.mRules,
                    out.mDiagnostics)) {
                out.mSucceeded = false;
                return false;
            }
        }

        out.mSucceeded = true;
        return true;
    }

    auto BuildDefaultPermutationValues(const FShaderPermutationLayout& layout)
        -> FShaderPermutationValues {
        FShaderPermutationValues values;
        values.mValues.Resize(layout.mDimensions.Size());
        for (usize i = 0; i < layout.mDimensions.Size(); ++i) {
            values.mValues[i] = layout.mDimensions[i].mDefaultValue;
        }
        return values;
    }

    auto BuildDefaultBuiltinValues(const FShaderBuiltinLayout& layout) -> FShaderBuiltinValues {
        FShaderBuiltinValues values;
        values.mValues.Resize(layout.mBuiltins.Size());
        for (usize i = 0; i < layout.mBuiltins.Size(); ++i) {
            values.mValues[i] = layout.mBuiltins[i].mDefaultValue;
        }
        return values;
    }

    auto ExpandMultiPermutationValues(const FShaderPermutationLayout& layout,
        TVector<FShaderPermutationValues>& outValues, usize maxPermutations) -> bool {
        outValues.Clear();
        if (maxPermutations == 0U) {
            return false;
        }
        FShaderPermutationValues base = BuildDefaultPermutationValues(layout);
        outValues.PushBack(base);

        for (usize dimIndex = 0; dimIndex < layout.mDimensions.Size(); ++dimIndex) {
            const auto& dim = layout.mDimensions[dimIndex];
            if (dim.mDomain == EShaderPermutationDomain::Feature) {
                continue;
            }
            TVector<i32> allowed;
            if (!GetDimensionValues(dim, allowed)) {
                return false;
            }
            TVector<FShaderPermutationValues> expanded;
            expanded.Reserve(outValues.Size() * allowed.Size());
            for (const auto& entry : outValues) {
                for (const auto value : allowed) {
                    FShaderPermutationValues copy = entry;
                    copy.mValues[dimIndex]        = value;
                    expanded.PushBack(copy);
                    if (expanded.Size() > maxPermutations) {
                        return false;
                    }
                }
            }
            outValues = Move(expanded);
        }
        return true;
    }

    auto EvaluateShaderPermutationRules(const FShaderPermutationRuleSet& rules,
        const FShaderPermutationLayout& layout, const FShaderPermutationValues& values,
        const FShaderBuiltinLayout* builtinLayout, const FShaderBuiltinValues* builtinValues,
        FString* outDiagnostics) -> bool {
        if (layout.mDimensions.Size() != values.mValues.Size()) {
            if (outDiagnostics) {
                AppendDiagnostic(*outDiagnostics, TEXT("Permutation value count mismatch."));
            }
            return false;
        }
        if ((builtinLayout != nullptr) && (builtinValues != nullptr)
            && (builtinLayout->mBuiltins.Size() != builtinValues->mValues.Size())) {
            if (outDiagnostics) {
                AppendDiagnostic(*outDiagnostics, TEXT("Builtin value count mismatch."));
            }
            return false;
        }

        THashMap<FString, i32> env;
        env.reserve(layout.mDimensions.Size()
            + ((builtinLayout != nullptr) ? builtinLayout->mBuiltins.Size() : 0));
        for (usize i = 0; i < layout.mDimensions.Size(); ++i) {
            const auto& dim   = layout.mDimensions[i];
            const auto  value = values.mValues[i];
            if (!IsValueAllowed(dim, value)) {
                if (outDiagnostics) {
                    AppendDiagnostic(*outDiagnostics, TEXT("Permutation value out of range."));
                }
                return false;
            }
            env[dim.mName] = value;
        }
        if ((builtinLayout != nullptr) && (builtinValues != nullptr)) {
            for (usize i = 0; i < builtinLayout->mBuiltins.Size(); ++i) {
                const auto& builtin = builtinLayout->mBuiltins[i];
                const auto  value   = builtinValues->mValues[i];
                if (!IsBuiltinAllowed(builtin, value)) {
                    if (outDiagnostics) {
                        AppendDiagnostic(*outDiagnostics, TEXT("Builtin value out of range."));
                    }
                    return false;
                }
                env[builtin.mName] = value;
            }
        }

        for (const auto& letRule : rules.mLets) {
            if (env.find(letRule.mName) != env.end()) {
                if (outDiagnostics) {
                    AppendDiagnostic(*outDiagnostics, TEXT("Let rule name conflicts with symbol."));
                }
                return false;
            }
            i32 value = 0;
            if (!EvaluateExpressionNode(
                    rules, letRule.mExpressionIndex, env, value, outDiagnostics)) {
                return false;
            }
            env[letRule.mName] = value;
        }

        for (const auto exprIndex : rules.mRequires) {
            i32 value = 0;
            if (!EvaluateExpressionNode(rules, exprIndex, env, value, outDiagnostics)) {
                return false;
            }
            if (value == 0) {
                return false;
            }
        }

        return true;
    }

    auto BuildPermutationDefines(const FShaderPermutationLayout& layout,
        const FShaderPermutationValues& values, FStringView prefix) -> TVector<FShaderMacro> {
        TVector<FShaderMacro> defines;
        if (layout.mDimensions.Size() != values.mValues.Size()) {
            return defines;
        }
        defines.Reserve(layout.mDimensions.Size());
        for (usize i = 0; i < layout.mDimensions.Size(); ++i) {
            const auto&  dim = layout.mDimensions[i];
            FShaderMacro macro;
            macro.mName.Assign(prefix);
            macro.mName.Append(dim.mName);
            macro.mValue = FString::ToString(values.mValues[i]);
            defines.PushBack(macro);
        }
        return defines;
    }

    auto BuildBuiltinDefines(const FShaderBuiltinLayout& layout, const FShaderBuiltinValues& values,
        FStringView prefix) -> TVector<FShaderMacro> {
        TVector<FShaderMacro> defines;
        if (layout.mBuiltins.Size() != values.mValues.Size()) {
            return defines;
        }
        defines.Reserve(layout.mBuiltins.Size());
        for (usize i = 0; i < layout.mBuiltins.Size(); ++i) {
            const auto&  builtin = layout.mBuiltins[i];
            FShaderMacro macro;
            if (builtin.mName.StartsWith(prefix)) {
                macro.mName.Assign(builtin.mName);
            } else {
                macro.mName.Assign(prefix);
                macro.mName.Append(builtin.mName);
            }
            macro.mValue = FString::ToString(values.mValues[i]);
            defines.PushBack(macro);
        }
        return defines;
    }

} // namespace AltinaEngine::ShaderCompiler
