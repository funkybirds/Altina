#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_ASSET_BUILD)
    #define AE_ASSET_API AE_DLLEXPORT
#else
    #define AE_ASSET_API AE_DLLIMPORT
#endif
