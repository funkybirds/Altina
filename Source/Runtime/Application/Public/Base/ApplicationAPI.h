#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_APPLICATION_BUILD)
    #define AE_APPLICATION_API AE_DLLEXPORT
#else
    #define AE_APPLICATION_API AE_DLLIMPORT
#endif
