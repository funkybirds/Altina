#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_IMAGING_BUILD)
    #define AE_IMAGING_API AE_DLLEXPORT
#else
    #define AE_IMAGING_API AE_DLLIMPORT
#endif
