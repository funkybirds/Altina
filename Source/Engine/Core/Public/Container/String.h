#pragma once

#include <initializer_list>
#include "../Algorithm/CStringUtils.h"

#include "StringView.h"
#include "Vector.h"
#include "../Types/Aliases.h"

namespace AltinaEngine::Core::Container
{

    class FString : public TVector<TChar>
    {
    public:
        using TSuper = TVector;
        using TSuper::TConstReference;
        using TSuper::TReference;
        using TSuper::TSizeType;
        using TSuper::TValueType;

        FString() = default;

        explicit FString(const TValueType* Text) { Append(Text); }

        FString(const TValueType* Text, usize Length) { Append(Text, Length); }

        FString(std::initializer_list<TValueType> Init)
        {
            Reserve(Init.size());
            for (const auto& character : Init)
            {
                this->PushBack(character);
            }
        }

        auto operator=(const TValueType* Text) -> FString&
        {
            TSuper::Clear();
            Append(Text);
            return *this;
        }

        void Assign(const TValueType* Text)
        {
            TSuper::Clear();
            Append(Text);
        }

        void Append(const TValueType* Text)
        {
            if (Text == nullptr)
            {
                return;
            }

            Append(Text, ComputeLength(Text));
        }

        void Append(const TValueType* Text, usize Length)
        {
            if ((Text == nullptr) || (Length == 0U))
            {
                return;
            }

            Reserve(this->Size() + Length);
            for (usize index = 0; index < Length; ++index)
            {
                this->PushBack(Text[index]);
            }
        }

        void               Append(TConstReference Character) { this->PushBack(Character); }

        [[nodiscard]] auto GetData() const noexcept -> const TValueType* { return TSuper::Data(); }
        auto               GetData() noexcept -> TValueType* { return TSuper::Data(); }

        [[nodiscard]] auto Length() const noexcept -> usize { return this->Size(); }

        [[nodiscard]] auto IsEmptyString() const noexcept -> bool { return this->IsEmpty(); }

        [[nodiscard]] auto ToView() const noexcept -> TStringView { return { this->GetData(), this->Length() }; }

        [[nodiscard]] operator TStringView() const noexcept { return ToView(); }

        void ToLower()
        {
            TransformCharacters([](TValueType Character) -> TValueType { return LowerChar(Character); });
        }

        void ToUpper()
        {
            TransformCharacters([](TValueType Character) -> TValueType { return UpperChar(Character); });
        }

        [[nodiscard]] auto ToLowerCopy() const -> FString
        {
            FString copy(*this);
            copy.ToLower();
            return copy;
        }

        [[nodiscard]] auto ToUpperCopy() const -> FString
        {
            FString copy(*this);
            copy.ToUpper();
            return copy;
        }

    private:
        template <typename Func> void TransformCharacters(Func&& Transformer)
        {
            for (usize index = 0; index < this->Size(); ++index)
            {
                (*this)[index] = Transformer((*this)[index]);
            }
        }

        static auto LowerChar(TValueType Character) -> TValueType
        {
            return AltinaEngine::Core::Algorithm::ToLowerChar<TValueType>(Character);
        }

        static auto UpperChar(TValueType Character) -> TValueType
        {
            return AltinaEngine::Core::Algorithm::ToUpperChar<TValueType>(Character);
        }

        static auto ComputeLength(const TValueType* Text) -> usize
        {
            usize length = 0U;
            while (Text[length] != static_cast<TValueType>(0))
            {
                ++length;
            }
            return length;
        }
    };
} // namespace AltinaEngine::Core::Container
