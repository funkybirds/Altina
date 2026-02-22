#pragma once

#if defined(__clang__)
    #define AE_REFL_ANNOTATE(payload) [[clang::annotate(payload)]]
#else
    #define AE_REFL_ANNOTATE(payload)
#endif

#ifndef AE_STRINGIFY_IMPL
    #define AE_STRINGIFY_IMPL(...) #__VA_ARGS__
#endif
#ifndef AE_STRINGIFY
    #define AE_STRINGIFY(...) AE_STRINGIFY_IMPL(__VA_ARGS__)
#endif

#ifndef ACLASS
    #define ACLASS(...) AE_REFL_ANNOTATE("AE.Class(" AE_STRINGIFY(__VA_ARGS__) ")")
#endif
#ifndef APROPERTY
    #define APROPERTY(...) AE_REFL_ANNOTATE("AE.Property(" AE_STRINGIFY(__VA_ARGS__) ")")
#endif
#ifndef AFUNCTION
    #define AFUNCTION(...) AE_REFL_ANNOTATE("AE.Function(" AE_STRINGIFY(__VA_ARGS__) ")")
#endif
