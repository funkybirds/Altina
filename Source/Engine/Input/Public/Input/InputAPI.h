#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_INPUT_BUILD)
    #define AE_INPUT_API AE_DLLEXPORT
#else
    #define AE_INPUT_API AE_DLLIMPORT
#endif
