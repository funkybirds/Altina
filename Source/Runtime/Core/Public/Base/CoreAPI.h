#pragma once

#include "AltinaBase.h"

#if defined(AE_CORE_BUILD)
    #define AE_CORE_API AE_DLLEXPORT
#else
    #define AE_CORE_API AE_DLLIMPORT
#endif