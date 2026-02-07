#pragma once

#include "../Base/CoreAPI.h"
#include "../Container/String.h"
#include "../Container/Vector.h"
#include "../Types/Aliases.h"

namespace AltinaEngine::Core::Platform {
    using Container::FString;
    using Container::TVector;

    struct FProcessOutput {
        bool    mSucceeded = false;
        u32     mExitCode  = 0;
        FString mOutput;
    };

    AE_CORE_API auto RunProcess(const FString& exePath, const TVector<FString>& args)
        -> FProcessOutput;
} // namespace AltinaEngine::Core::Platform
