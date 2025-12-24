#pragma once

#include "Vector.h"
#include "../Types/Aliases.h"

namespace AltinaEngine::Core::Container
{
    
    template <typename CharType>
    class TBasicStringView
    {
    public:
        using value_type = CharType;
        using pointer = const value_type*;
        using reference = const value_type&;
        using size_type = usize;

        constexpr TBasicStringView() noexcept
            : mData(nullptr)
            , mLength(0)
        {
        }

        constexpr TBasicStringView(pointer data, size_type length) noexcept
            : mData(data)
            , mLength(length)
        {
        }

        constexpr TBasicStringView(const value_type* data) noexcept
            : mData(data)
            , mLength(ComputeLength(data))
        {
        }

        template <typename Allocator>
        TBasicStringView(const TVector<CharType, Allocator>& Vector) noexcept
            : mData(Vector.Data())
            , mLength(Vector.Size())
        {
        }

        [[nodiscard]] constexpr pointer Data() const noexcept { return mData; }
        [[nodiscard]] constexpr size_type Length() const noexcept { return mLength; }
        [[nodiscard]] constexpr bool IsEmpty() const noexcept { return mLength == 0; }

        [[nodiscard]] constexpr reference operator[](size_type index) const noexcept
        {
            return mData[index];
        }

        [[nodiscard]] constexpr pointer begin() const noexcept { return mData; }
        [[nodiscard]] constexpr pointer end() const noexcept { return mData + mLength; }

        [[nodiscard]] constexpr TBasicStringView Substring(size_type offset, size_type count) const noexcept
        {
            if (offset > mLength)
            {
                return {};
            }

            const size_type newLength = (offset + count > mLength) ? (mLength - offset) : count;
            return { mData + offset, newLength };
        }

    private:
        static constexpr size_type ComputeLength(const value_type* data) noexcept
        {
            size_type length = 0;
            if (data == nullptr)
            {
                return 0;
            }

            while (data[length] != static_cast<value_type>(0))
            {
                ++length;
            }
            return length;
        }

    private:
        pointer  mData;
        size_type mLength;
    };

    using TStringView = TBasicStringView<TChar>;

} // namespace AltinaEngine::Core::Container
