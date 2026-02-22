#pragma once

#include <functional>
#include <initializer_list>
#include <string>
#include "../Algorithm/CStringUtils.h"

#include "StringView.h"
#include "Vector.h"
#include "../Types/Aliases.h"
#include "../Types/Traits.h"

namespace AltinaEngine::Core::Container {

    template <typename T> class TBasicString : public TVector<T> {
    public:
        using TSuper = TVector<T>;
        using typename TSuper::TConstReference;
        using typename TSuper::TReference;
        using typename TSuper::TSizeType;
        using typename TSuper::TValueType;
        using TView = TBasicStringView<TValueType>;

        static constexpr TSizeType npos = static_cast<TSizeType>(-1);

        TBasicString() = default;

        explicit TBasicString(const TValueType* Text) { Append(Text); }

        TBasicString(const TValueType* Text, usize Length) { Append(Text, Length); }

        TBasicString(std::initializer_list<TValueType> Init) {
            Reserve(Init.size());
            for (const auto& character : Init) {
                this->PushBack(character);
            }
        }

        TBasicString(TBasicStringView<T> strView) { Append(strView.Data(), strView.Length()); }

        auto operator=(const TValueType* Text) -> TBasicString& {
            TSuper::Clear();
            Append(Text);
            return *this;
        }

        void Assign(const TValueType* Text) {
            TSuper::Clear();
            Append(Text);
        }

        void Assign(TBasicStringView<T> Text) {
            TSuper::Clear();
            Append(Text);
        }

        void Assign(const TBasicString& Text) {
            TSuper::Clear();
            Append(Text);
        }

        void Append(const TValueType* Text) {
            if (Text == nullptr) {
                return;
            }

            Append(Text, ComputeLength(Text));
        }

        void Append(const TValueType* Text, usize Length) {
            if ((Text == nullptr) || (Length == 0U)) {
                return;
            }

            this->Reserve(this->Size() + Length);
            for (usize index = 0; index < Length; ++index) {
                this->PushBack(Text[index]);
            }
        }

        void Append(TConstReference Character) { this->PushBack(Character); }

        void Append(TBasicStringView<T> Text) {
            if (Text.Length() == 0) {
                return;
            }
            const auto* data = Text.Data();
            const auto  length = Text.Length();
            const auto* base = this->GetData();
            if (base != nullptr && data >= base && data < (base + this->Length())) {
                TBasicString temp(Text);
                Append(temp.GetData(), temp.Length());
                return;
            }
            Append(data, length);
        }

        void Append(const TBasicString& Text) { Append(Text.ToView()); }

        template <typename NumberT>
        void AppendNumber(NumberT value) {
            if constexpr (AltinaEngine::TTypeIsIntegral<NumberT>::Value
                || AltinaEngine::TTypeIsFloatingPoint<NumberT>::Value) {
                AppendNumberImpl(value);
            } else {
                static_assert(AltinaEngine::TTypeIsIntegral<NumberT>::Value
                    || AltinaEngine::TTypeIsFloatingPoint<NumberT>::Value,
                    "AppendNumber only supports scalar numeric types.");
            }
        }

        template <typename NumberT>
        [[nodiscard]] static auto ToString(NumberT value) -> TBasicString {
            TBasicString out;
            out.AppendNumber(value);
            return out;
        }

        [[nodiscard]] auto GetData() const noexcept -> const TValueType* { return TSuper::Data(); }
        auto               GetData() noexcept -> TValueType* { return TSuper::Data(); }

        [[nodiscard]] auto Length() const noexcept -> usize { return this->Size(); }

        [[nodiscard]] auto IsEmptyString() const noexcept -> bool { return this->IsEmpty(); }

        [[nodiscard]] auto ToView() const noexcept -> TView {
            return { this->GetData(), this->Length() };
        }

        [[nodiscard]] operator TView() const noexcept { return ToView(); }

        [[nodiscard]] auto CStr() const noexcept -> const TValueType* {
            if (this->IsEmpty()) {
                return EmptyString();
            }
            const_cast<TBasicString*>(this)->EnsureNullTerminated();
            return this->GetData();
        }

        [[nodiscard]] auto CStr() noexcept -> TValueType* {
            EnsureNullTerminated();
            return this->GetData();
        }

        void EnsureNullTerminated() {
            const TSizeType length = this->Size();
            if (this->Capacity() < length + 1) {
                this->Reserve(length + 1);
            }
            if (this->GetData() != nullptr) {
                this->GetData()[length] = static_cast<TValueType>(0);
            }
        }

        [[nodiscard]] auto Compare(TView other) const noexcept -> int {
            return ToView().Compare(other);
        }

        [[nodiscard]] auto StartsWith(TView prefix) const noexcept -> bool {
            return ToView().StartsWith(prefix);
        }

        [[nodiscard]] auto EndsWith(TView suffix) const noexcept -> bool {
            return ToView().EndsWith(suffix);
        }

        [[nodiscard]] auto Contains(TView needle) const noexcept -> bool {
            return ToView().Contains(needle);
        }

        [[nodiscard]] auto Contains(TValueType value) const noexcept -> bool {
            return ToView().Contains(value);
        }

        [[nodiscard]] auto Find(TView needle, TSizeType pos = 0) const noexcept -> TSizeType {
            return ToView().Find(needle, pos);
        }

        [[nodiscard]] auto Find(TValueType value, TSizeType pos = 0) const noexcept -> TSizeType {
            return ToView().Find(value, pos);
        }

        [[nodiscard]] auto RFind(TView needle, TSizeType pos = npos) const noexcept -> TSizeType {
            return ToView().RFind(needle, pos);
        }

        [[nodiscard]] auto RFind(TValueType value, TSizeType pos = npos) const noexcept
            -> TSizeType {
            return ToView().RFind(value, pos);
        }

        [[nodiscard]] auto FindFirstOf(TView set, TSizeType pos = 0) const noexcept -> TSizeType {
            return ToView().FindFirstOf(set, pos);
        }

        [[nodiscard]] auto FindLastOf(TView set, TSizeType pos = npos) const noexcept -> TSizeType {
            return ToView().FindLastOf(set, pos);
        }

        [[nodiscard]] auto FindFirstNotOf(TView set, TSizeType pos = 0) const noexcept
            -> TSizeType {
            return ToView().FindFirstNotOf(set, pos);
        }

        [[nodiscard]] auto FindLastNotOf(TView set, TSizeType pos = npos) const noexcept
            -> TSizeType {
            return ToView().FindLastNotOf(set, pos);
        }

        [[nodiscard]] auto Substr(TSizeType offset, TSizeType count = npos) const -> TBasicString {
            TBasicString out;
            const auto   view = ToView().Substr(offset, count);
            out.Append(view);
            return out;
        }

        [[nodiscard]] auto SubstrView(TSizeType offset, TSizeType count = npos) const noexcept
            -> TView {
            return ToView().Substr(offset, count);
        }

        [[nodiscard]] auto operator==(const TBasicString& other) const noexcept -> bool {
            return ToView() == other.ToView();
        }

        [[nodiscard]] auto operator!=(const TBasicString& other) const noexcept -> bool {
            return ToView() != other.ToView();
        }

        [[nodiscard]] auto operator==(TView other) const noexcept -> bool {
            return ToView() == other;
        }

        [[nodiscard]] auto operator!=(TView other) const noexcept -> bool {
            return ToView() != other;
        }

        [[nodiscard]] auto operator<(TView other) const noexcept -> bool {
            return ToView() < other;
        }

        auto operator+=(TView other) -> TBasicString& {
            Append(other);
            return *this;
        }

        auto operator+=(const TBasicString& other) -> TBasicString& {
            Append(other);
            return *this;
        }

        auto operator+=(const TValueType* other) -> TBasicString& {
            Append(other);
            return *this;
        }

        friend auto operator+(TBasicString lhs, TView rhs) -> TBasicString {
            lhs.Append(rhs);
            return lhs;
        }

        void          ToLower() {
            TransformCharacters(
                [](TValueType Character) -> TValueType { return LowerChar(Character); });
        }

        void ToUpper() {
            TransformCharacters(
                [](TValueType Character) -> TValueType { return UpperChar(Character); });
        }

        [[nodiscard]] auto ToLowerCopy() const -> TBasicString {
            TBasicString copy(*this);
            copy.ToLower();
            return copy;
        }

        [[nodiscard]] auto ToUpperCopy() const -> TBasicString {
            TBasicString copy(*this);
            copy.ToUpper();
            return copy;
        }

    private:
        static constexpr auto EmptyString() noexcept -> const TValueType* {
            static constexpr TValueType kEmpty[1] = { static_cast<TValueType>(0) };
            return kEmpty;
        }

        template <typename Func> void TransformCharacters(Func&& Transformer) {
            for (usize index = 0; index < this->Size(); ++index) {
                (*this)[index] = Transformer((*this)[index]);
            }
        }

        static auto LowerChar(TValueType Character) -> TValueType {
            return AltinaEngine::Core::Algorithm::ToLowerChar<TValueType>(Character);
        }

        static auto UpperChar(TValueType Character) -> TValueType {
            return AltinaEngine::Core::Algorithm::ToUpperChar<TValueType>(Character);
        }

        static auto ComputeLength(const TValueType* Text) -> usize {
            usize length = 0U;
            while (Text[length] != static_cast<TValueType>(0)) {
                ++length;
            }
            return length;
        }

        template <typename NumberT> void AppendNumberImpl(NumberT value) {
            if constexpr (AltinaEngine::CSameAs<TValueType, char>) {
                const auto text = std::to_string(value);
                Append(text.c_str(), static_cast<usize>(text.size()));
            } else if constexpr (AltinaEngine::CSameAs<TValueType, wchar_t>) {
                const auto text = std::to_wstring(value);
                Append(text.c_str(), static_cast<usize>(text.size()));
            } else {
                const auto text = std::to_string(value);
                this->Reserve(this->Size() + text.size());
                for (char c : text) {
                    this->PushBack(static_cast<TValueType>(c));
                }
            }
        }
    };
    using FString       = TBasicString<TChar>;
    using FNativeString = TBasicString<char>;
} // namespace AltinaEngine::Core::Container

namespace std {
    template <typename CharT>
    struct hash<AltinaEngine::Core::Container::TBasicStringView<CharT>> {
        auto operator()(
            const AltinaEngine::Core::Container::TBasicStringView<CharT>& value) const noexcept
            -> size_t {
            constexpr size_t kOffset =
                (sizeof(size_t) == 8) ? 14695981039346656037ull : 2166136261u;
            constexpr size_t kPrime =
                (sizeof(size_t) == 8) ? 1099511628211ull : 16777619u;

            size_t hash = kOffset;
            const auto* data = value.Data();
            const auto  length = value.Length();
            for (size_t i = 0; i < static_cast<size_t>(length); ++i) {
                hash ^= static_cast<size_t>(
                    static_cast<typename AltinaEngine::Core::Container::TBasicStringView<
                        CharT>::TUnsigned>(data[i]));
                hash *= kPrime;
            }
            return hash;
        }
    };

    template <typename CharT>
    struct hash<AltinaEngine::Core::Container::TBasicString<CharT>> {
        auto operator()(
            const AltinaEngine::Core::Container::TBasicString<CharT>& value) const noexcept
            -> size_t {
            const auto view = AltinaEngine::Core::Container::TBasicStringView<CharT>(
                value.GetData(), value.Length());
            return std::hash<AltinaEngine::Core::Container::TBasicStringView<CharT>>{}(view);
        }
    };
} // namespace std
