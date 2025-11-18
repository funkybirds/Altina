#pragma

#include "../Generic/GenericPlatformDecl.h"

#if AE_PLATFORM_WIN
    #include "../Windows/PlatformIntrinsicWindows.h"
#else
    #error "PlatformIntrinsic.h not implemented for this platform"
#endif