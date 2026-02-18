#pragma once

#include "Types/Aliases.h"

namespace AltinaEngine::Scripting {
    using AltinaEngine::i32;
    using AltinaEngine::u64;

    struct FScriptHandle {
        void* mPointer = nullptr;

        [[nodiscard]] auto IsValid() const noexcept -> bool { return mPointer != nullptr; }
    };

    struct FScriptInvocation {
        void* mArgs = nullptr;
        i32   mSize = 0;
    };

    using FScriptTypeId = u64;
} // namespace AltinaEngine::Scripting
