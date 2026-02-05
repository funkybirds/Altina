#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_LAUNCH_BUILD)
    #define AE_LAUNCH_API AE_DLLEXPORT
#else
    #define AE_LAUNCH_API AE_DLLIMPORT
#endif
