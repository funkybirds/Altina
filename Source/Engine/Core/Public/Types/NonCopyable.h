#pragma once

namespace AltinaEngine {

    // Base class to disable copy semantics for classes
    class FNonCopyableClass {
    public:
        FNonCopyableClass()          = default;
        virtual ~FNonCopyableClass() = default;

        FNonCopyableClass(const FNonCopyableClass&)                    = delete;
        FNonCopyableClass(FNonCopyableClass&&)                         = default;
        auto operator=(const FNonCopyableClass&) -> FNonCopyableClass& = delete;
        auto operator=(FNonCopyableClass&&) -> FNonCopyableClass&      = default;
    };

    // Base struct to disable copy semantics for POD-like structs
    struct FNonCopyableStruct {
        FNonCopyableStruct()  = default;
        ~FNonCopyableStruct() = default;

        FNonCopyableStruct(const FNonCopyableStruct&)                    = delete;
        FNonCopyableStruct(FNonCopyableStruct&&)                         = default;
        auto operator=(const FNonCopyableStruct&) -> FNonCopyableStruct& = delete;
        auto operator=(FNonCopyableStruct&&) -> FNonCopyableStruct&      = default;
    };

    struct FNonMovableStruct {
        FNonMovableStruct()                                            = default;
        FNonMovableStruct(FNonMovableStruct const&)                    = delete;
        FNonMovableStruct(FNonMovableStruct&&)                         = delete;
        auto operator=(FNonMovableStruct const&) -> FNonMovableStruct& = delete;
        auto operator=(FNonMovableStruct&&) -> FNonMovableStruct&      = delete;
        ~FNonMovableStruct()                                           = default;
    };
    class FNonMovableClass {
    public:
        FNonMovableClass()                                           = default;
        ~FNonMovableClass()                                          = default;
        FNonMovableClass(const FNonMovableClass&)                    = delete;
        FNonMovableClass(FNonMovableClass&&)                         = delete;
        auto operator=(const FNonMovableClass&) -> FNonMovableClass& = delete;
        auto operator=(FNonMovableClass&&) -> FNonMovableClass&      = delete;
    };

    // Backwards-compatible aliases (older tests and code expect these names)
    using NonCopyableClass  = FNonCopyableClass;
    using NonCopyableStruct = FNonCopyableStruct;
    using NonMovableStruct  = FNonMovableStruct;
    using NonMovableClass   = FNonMovableClass;

} // namespace AltinaEngine
