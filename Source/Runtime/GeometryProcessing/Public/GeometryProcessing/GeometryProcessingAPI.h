#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_GEOMETRY_PROCESSING_BUILD)
    #define AE_GEOMETRY_PROCESSING_API AE_DLLEXPORT
#else
    #define AE_GEOMETRY_PROCESSING_API AE_DLLIMPORT
#endif
