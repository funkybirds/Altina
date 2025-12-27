#pragma once

namespace AltinaEngine
{

    // Base class to disable copy semantics for classes
    class NonCopyableClass
    {
    public:
        NonCopyableClass()  = default;
        ~NonCopyableClass() = default;

        NonCopyableClass(const NonCopyableClass&)                    = delete;
        auto operator=(const NonCopyableClass&) -> NonCopyableClass& = delete;
    };

    // Base struct to disable copy semantics for POD-like structs
    struct NonCopyableStruct
    {
        NonCopyableStruct()  = default;
        ~NonCopyableStruct() = default;

        NonCopyableStruct(const NonCopyableStruct&)                    = delete;
        auto operator=(const NonCopyableStruct&) -> NonCopyableStruct& = delete;
    };

} // namespace AltinaEngine
