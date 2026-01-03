#pragma once

#include <cstddef>
#include <cstdint>

namespace AltinaEngine {

    using i8  = std::int8_t;  // NOLINT(*-identifier-naming)
    using i16 = std::int16_t; // NOLINT(*-identifier-naming)
    using i32 = std::int32_t; // NOLINT(*-identifier-naming)
    using i64 = std::int64_t; // NOLINT(*-identifier-naming)

    using u8  = std::uint8_t;  // NOLINT(*-identifier-naming)
    using u16 = std::uint16_t; // NOLINT(*-identifier-naming)
    using u32 = std::uint32_t; // NOLINT(*-identifier-naming)
    using u64 = std::uint64_t; // NOLINT(*-identifier-naming)

    using f32 = float;  // NOLINT(*-identifier-naming)
    using f64 = double; // NOLINT(*-identifier-naming)

    using usize = std::size_t;    // NOLINT(*-identifier-naming)
    using isize = std::ptrdiff_t; // NOLINT(*-identifier-naming)

#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
    using TChar = wchar_t;
    #ifndef TEXT
        #define TEXT(str) L##str
    #endif
#else
    using TChar = char;
    #ifndef TEXT
        #define TEXT(str) str
    #endif
#endif

} // namespace AltinaEngine
