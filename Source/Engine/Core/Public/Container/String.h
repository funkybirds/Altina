#pragma once

#include <cctype>
#include <cwctype>
#include <initializer_list>

#include "StringView.h"
#include "Vector.h"
#include "../Types/Aliases.h"

namespace AltinaEngine::Core::Container
{
    class FString : public TVector<AltinaEngine::TChar>
    {
    public:
        using Super = TVector<AltinaEngine::TChar>;
        using typename Super::value_type;
        using typename Super::size_type;
        using typename Super::reference;
        using typename Super::const_reference;

        FString() = default;

        explicit FString(const value_type* Text)
        {
            Append(Text);
        }

        FString(const value_type* Text, AltinaEngine::usize Length)
        {
            Append(Text, Length);
        }

        FString(std::initializer_list<value_type> Init)
        {
            Super::Reserve(static_cast<size_type>(Init.size()));
            for (const auto& Character : Init)
            {
                this->PushBack(Character);
            }
        }

        FString& operator=(const value_type* Text)
        {
            Super::Clear();
            Append(Text);
            return *this;
        }

        void Assign(const value_type* Text)
        {
            Super::Clear();
            Append(Text);
        }

        void Append(const value_type* Text)
        {
            if (Text == nullptr)
            {
                return;
            }

            Append(Text, ComputeLength(Text));
        }

        void Append(const value_type* Text, AltinaEngine::usize Length)
        {
            if ((Text == nullptr) || (Length == 0U))
            {
                return;
            }

            Super::Reserve(this->Size() + Length);
            for (AltinaEngine::usize Index = 0; Index < Length; ++Index)
            {
                this->PushBack(Text[Index]);
            }
        }

        void Append(const_reference Character)
        {
            this->PushBack(Character);
        }

        const value_type* GetData() const noexcept { return Super::Data(); }
        value_type*       GetData() noexcept { return Super::Data(); }

        AltinaEngine::usize Length() const noexcept { return this->Size(); }

        bool IsEmptyString() const noexcept { return this->IsEmpty(); }

        [[nodiscard]] TStringView ToView() const noexcept
        {
            return TStringView(this->GetData(), this->Length());
        }

        [[nodiscard]] operator TStringView() const noexcept
        {
            return ToView();
        }

        void ToLower()
        {
            TransformCharacters([](value_type Character) { return LowerChar(Character); });
        }

        void ToUpper()
        {
            TransformCharacters([](value_type Character) { return UpperChar(Character); });
        }

        FString ToLowerCopy() const
        {
            FString Copy(*this);
            Copy.ToLower();
            return Copy;
        }

        FString ToUpperCopy() const
        {
            FString Copy(*this);
            Copy.ToUpper();
            return Copy;
        }

    private:
        template <typename Func>
        void TransformCharacters(Func&& Transformer)
        {
            for (AltinaEngine::usize Index = 0; Index < this->Size(); ++Index)
            {
                (*this)[Index] = Transformer((*this)[Index]);
            }
        }

        static value_type LowerChar(value_type Character)
        {
            if constexpr (sizeof(value_type) == sizeof(wchar_t))
            {
                return static_cast<value_type>(std::towlower(static_cast<wint_t>(Character)));
            }
            else
            {
                const unsigned char Narrow = static_cast<unsigned char>(Character);
                return static_cast<value_type>(std::tolower(Narrow));
            }
        }

        static value_type UpperChar(value_type Character)
        {
            if constexpr (sizeof(value_type) == sizeof(wchar_t))
            {
                return static_cast<value_type>(std::towupper(static_cast<wint_t>(Character)));
            }
            else
            {
                const unsigned char Narrow = static_cast<unsigned char>(Character);
                return static_cast<value_type>(std::toupper(Narrow));
            }
        }

        static AltinaEngine::usize ComputeLength(const value_type* Text)
        {
            AltinaEngine::usize Length = 0U;
            while (Text[Length] != static_cast<value_type>(0))
            {
                ++Length;
            }
            return Length;
        }
    };
}
