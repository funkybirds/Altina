#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_EDITOR_VIEWPORT_BUILD)
    #define AE_EDITOR_VIEWPORT_API AE_DLLEXPORT
#else
    #define AE_EDITOR_VIEWPORT_API AE_DLLIMPORT
#endif
