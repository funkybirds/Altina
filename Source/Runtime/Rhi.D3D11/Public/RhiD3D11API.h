#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_RHI_D3D11_BUILD)
    #define AE_RHI_D3D11_API AE_DLLEXPORT
#else
    #define AE_RHI_D3D11_API AE_DLLIMPORT
#endif
