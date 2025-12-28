#pragma once

#define AE_RTTI_ENABLED 1

#ifdef AE_RTTI_ENABLED
    #include <typeinfo>
#endif

namespace AltinaEngine
{
#ifdef AE_RTTI_ENABLED
    using FTypeInfo = std::type_info; // NOLINT
#else
    using FTypeInfo = void; // NOLINT
#endif
} // namespace AltinaEngine