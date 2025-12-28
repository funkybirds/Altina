#pragma once

#include "Vector.h"
#include "../Types/Aliases.h"

namespace AltinaEngine::Core::Container
{

    template <typename CharType> class TBasicStringView
    {
    public:
        using TValueType = CharType;
        using TPointer   = const TValueType*;
        using TReference = const TValueType&;
        using TSizeType  = usize;

        constexpr TBasicStringView() noexcept : mData(nullptr), mLength(0) {}

        constexpr TBasicStringView(TPointer data, TSizeType length) noexcept : mData(data), mLength(length) {}

        constexpr TBasicStringView(const TValueType* data) noexcept : mData(data), mLength(ComputeLength(data)) {}

        template <typename Allocator>
        TBasicStringView(const TVector<CharType, Allocator>& Vector) noexcept
            : mData(Vector.Data()), mLength(Vector.Size())
        {
        }

        [[nodiscard]] constexpr auto Data() const noexcept -> TPointer { return mData; }
        [[nodiscard]] constexpr auto Length() const noexcept -> TSizeType { return mLength; }
        [[nodiscard]] constexpr auto IsEmpty() const noexcept -> bool { return mLength == 0; }

        [[nodiscard]] constexpr auto operator[](TSizeType index) const noexcept -> TReference { return mData[index]; }

        [[nodiscard]] constexpr auto begin() const noexcept -> TPointer { return mData; } // NOLINT(*-identifier-naming)
        [[nodiscard]] constexpr auto end() const noexcept -> TPointer
        {
            return mData + mLength;
        } // NOLINT(*-identifier-naming)

        [[nodiscard]] constexpr auto Substring(TSizeType offset, TSizeType count) const noexcept -> TBasicStringView
        {
            if (offset > mLength)
            {
                return {};
            }

            const TSizeType newLength = (offset + count > mLength) ? (mLength - offset) : count;
            return { mData + offset, newLength };
        }

    private:
        static constexpr auto ComputeLength(const TValueType* data) noexcept -> TSizeType
        {
            TSizeType length = 0;
            if (data == nullptr)
            {
                return 0;
            }

            while (data[length] != static_cast<TValueType>(0))
            {
                ++length;
            }
            return length;
        }

    private:
        TPointer  mData;
        TSizeType mLength;
    };

    using TStringView       = TBasicStringView<TChar>;
    using TNativeStringView = TBasicStringView<char>;

} // namespace AltinaEngine::Core::Container
