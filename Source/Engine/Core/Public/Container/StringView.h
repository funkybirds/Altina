#pragma once

#include <type_traits>

#include "Vector.h"
#include "../Types/Aliases.h"

namespace AltinaEngine::Core::Container {

    template <typename CharType> class TBasicStringView {
    public:
        using TValueType = CharType;
        using TPointer   = const TValueType*;
        using TReference = const TValueType&;
        using TSizeType  = usize;
        using TUnsigned =
            std::conditional_t<std::is_integral_v<TValueType>, std::make_unsigned_t<TValueType>,
                TValueType>;

        static constexpr TSizeType npos = static_cast<TSizeType>(-1);

        constexpr TBasicStringView() noexcept : mData(nullptr), mLength(0) {}

        constexpr TBasicStringView(TPointer data, TSizeType length) noexcept
            : mData(data), mLength(length) {}

        constexpr TBasicStringView(const TValueType* data) noexcept
            : mData(data), mLength(ComputeLength(data)) {}

        template <typename Allocator>
        TBasicStringView(const TVector<CharType, Allocator>& Vector) noexcept
            : mData(Vector.Data()), mLength(Vector.Size()) {}

        [[nodiscard]] constexpr auto Data() const noexcept -> TPointer { return mData; }
        [[nodiscard]] constexpr auto Length() const noexcept -> TSizeType { return mLength; }
        [[nodiscard]] constexpr auto IsEmpty() const noexcept -> bool { return mLength == 0; }

        [[nodiscard]] constexpr auto operator[](TSizeType index) const noexcept -> TReference {
            return mData[index];
        }

        [[nodiscard]] constexpr auto begin() const noexcept -> TPointer {
            return mData;
        } // NOLINT(*-identifier-naming)
        [[nodiscard]] constexpr auto end() const noexcept -> TPointer // NOLINT(*-identifier-naming)
        {
            return mData + mLength;
        }

        [[nodiscard]] constexpr auto Substring(TSizeType offset, TSizeType count) const noexcept
            -> TBasicStringView {
            if (offset > mLength) {
                return {};
            }

            const TSizeType newLength = (offset + count > mLength) ? (mLength - offset) : count;
            return { mData + offset, newLength };
        }

        [[nodiscard]] constexpr auto Substr(TSizeType offset, TSizeType count = npos) const noexcept
            -> TBasicStringView {
            if (count == npos) {
                return Substring(offset, mLength);
            }
            return Substring(offset, count);
        }

        [[nodiscard]] constexpr auto Compare(TBasicStringView other) const noexcept -> int {
            const TSizeType minLength = (mLength < other.mLength) ? mLength : other.mLength;
            for (TSizeType i = 0; i < minLength; ++i) {
                const auto a = Normalize(mData[i]);
                const auto b = Normalize(other.mData[i]);
                if (a < b) {
                    return -1;
                }
                if (a > b) {
                    return 1;
                }
            }
            if (mLength < other.mLength) {
                return -1;
            }
            if (mLength > other.mLength) {
                return 1;
            }
            return 0;
        }

        [[nodiscard]] constexpr auto operator==(TBasicStringView other) const noexcept -> bool {
            if (mLength != other.mLength) {
                return false;
            }
            for (TSizeType i = 0; i < mLength; ++i) {
                if (mData[i] != other.mData[i]) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] constexpr auto operator!=(TBasicStringView other) const noexcept -> bool {
            return !(*this == other);
        }

        [[nodiscard]] constexpr auto operator<(TBasicStringView other) const noexcept -> bool {
            return Compare(other) < 0;
        }

        [[nodiscard]] constexpr auto StartsWith(TBasicStringView prefix) const noexcept -> bool {
            if (prefix.mLength > mLength) {
                return false;
            }
            for (TSizeType i = 0; i < prefix.mLength; ++i) {
                if (mData[i] != prefix.mData[i]) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] constexpr auto EndsWith(TBasicStringView suffix) const noexcept -> bool {
            if (suffix.mLength > mLength) {
                return false;
            }
            const TSizeType offset = mLength - suffix.mLength;
            for (TSizeType i = 0; i < suffix.mLength; ++i) {
                if (mData[offset + i] != suffix.mData[i]) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] constexpr auto Contains(TBasicStringView needle) const noexcept -> bool {
            return Find(needle) != npos;
        }

        [[nodiscard]] constexpr auto Contains(TValueType value) const noexcept -> bool {
            return Find(value) != npos;
        }

        [[nodiscard]] constexpr auto Find(TBasicStringView needle,
            TSizeType pos = 0) const noexcept -> TSizeType {
            if (needle.mLength == 0) {
                return (pos <= mLength) ? pos : npos;
            }
            if (pos >= mLength || needle.mLength > mLength) {
                return npos;
            }

            const TSizeType lastStart = mLength - needle.mLength;
            for (TSizeType i = pos; i <= lastStart; ++i) {
                bool match = true;
                for (TSizeType j = 0; j < needle.mLength; ++j) {
                    if (mData[i + j] != needle.mData[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    return i;
                }
            }
            return npos;
        }

        [[nodiscard]] constexpr auto Find(TValueType value,
            TSizeType pos = 0) const noexcept -> TSizeType {
            if (pos >= mLength) {
                return npos;
            }
            for (TSizeType i = pos; i < mLength; ++i) {
                if (mData[i] == value) {
                    return i;
                }
            }
            return npos;
        }

        [[nodiscard]] constexpr auto RFind(TBasicStringView needle,
            TSizeType pos = npos) const noexcept -> TSizeType {
            if (needle.mLength == 0) {
                if (mLength == 0) {
                    return 0;
                }
                return (pos == npos || pos >= mLength) ? mLength : pos;
            }
            if (needle.mLength > mLength) {
                return npos;
            }

            const TSizeType maxStart = mLength - needle.mLength;
            TSizeType       start    = (pos == npos || pos > maxStart) ? maxStart : pos;
            for (TSizeType i = start + 1; i-- > 0;) {
                bool match = true;
                for (TSizeType j = 0; j < needle.mLength; ++j) {
                    if (mData[i + j] != needle.mData[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    return i;
                }
                if (i == 0) {
                    break;
                }
            }
            return npos;
        }

        [[nodiscard]] constexpr auto RFind(TValueType value,
            TSizeType pos = npos) const noexcept -> TSizeType {
            if (mLength == 0) {
                return npos;
            }
            TSizeType start = (pos == npos || pos >= mLength) ? (mLength - 1) : pos;
            for (TSizeType i = start + 1; i-- > 0;) {
                if (mData[i] == value) {
                    return i;
                }
                if (i == 0) {
                    break;
                }
            }
            return npos;
        }

        [[nodiscard]] constexpr auto FindFirstOf(TBasicStringView set,
            TSizeType pos = 0) const noexcept -> TSizeType {
            if (set.mLength == 0 || pos >= mLength) {
                return npos;
            }
            for (TSizeType i = pos; i < mLength; ++i) {
                if (ContainsChar(set, mData[i])) {
                    return i;
                }
            }
            return npos;
        }

        [[nodiscard]] constexpr auto FindLastOf(TBasicStringView set,
            TSizeType pos = npos) const noexcept -> TSizeType {
            if (set.mLength == 0 || mLength == 0) {
                return npos;
            }
            TSizeType start = (pos == npos || pos >= mLength) ? (mLength - 1) : pos;
            for (TSizeType i = start + 1; i-- > 0;) {
                if (ContainsChar(set, mData[i])) {
                    return i;
                }
                if (i == 0) {
                    break;
                }
            }
            return npos;
        }

        [[nodiscard]] constexpr auto FindFirstNotOf(TBasicStringView set,
            TSizeType pos = 0) const noexcept -> TSizeType {
            if (pos >= mLength) {
                return npos;
            }
            if (set.mLength == 0) {
                return pos;
            }
            for (TSizeType i = pos; i < mLength; ++i) {
                if (!ContainsChar(set, mData[i])) {
                    return i;
                }
            }
            return npos;
        }

        [[nodiscard]] constexpr auto FindLastNotOf(TBasicStringView set,
            TSizeType pos = npos) const noexcept -> TSizeType {
            if (mLength == 0) {
                return npos;
            }
            if (set.mLength == 0) {
                return (pos == npos || pos >= mLength) ? (mLength - 1) : pos;
            }
            TSizeType start = (pos == npos || pos >= mLength) ? (mLength - 1) : pos;
            for (TSizeType i = start + 1; i-- > 0;) {
                if (!ContainsChar(set, mData[i])) {
                    return i;
                }
                if (i == 0) {
                    break;
                }
            }
            return npos;
        }

    private:
        static constexpr auto ComputeLength(const TValueType* data) noexcept -> TSizeType {
            TSizeType length = 0;
            if (data == nullptr) {
                return 0;
            }

            while (data[length] != static_cast<TValueType>(0)) {
                ++length;
            }
            return length;
        }

        static constexpr auto Normalize(TValueType value) noexcept -> TUnsigned {
            return static_cast<TUnsigned>(value);
        }

        static constexpr auto ContainsChar(TBasicStringView set, TValueType value) noexcept
            -> bool {
            for (TSizeType i = 0; i < set.mLength; ++i) {
                if (set.mData[i] == value) {
                    return true;
                }
            }
            return false;
        }

        TPointer  mData;
        TSizeType mLength;
    };

    using FStringView       = TBasicStringView<TChar>; // NOLINT
    using FNativeStringView = TBasicStringView<char>;  // NOLINT

} // namespace AltinaEngine::Core::Container
