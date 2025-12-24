#pragma once

namespace AltinaEngine
{

    // Base class to disable copy semantics for classes
    class NonCopyableClass
    {
    public:
        NonCopyableClass() = default;
        ~NonCopyableClass() = default;

        NonCopyableClass(const NonCopyableClass&) = delete;
        NonCopyableClass& operator=(const NonCopyableClass&) = delete;
    };

    // Base struct to disable copy semantics for POD-like structs
    struct NonCopyableStruct
    {
        NonCopyableStruct() = default;
        ~NonCopyableStruct() = default;

        NonCopyableStruct(const NonCopyableStruct&) = delete;
        NonCopyableStruct& operator=(const NonCopyableStruct&) = delete;
    };

} // namespace AltinaEngine
