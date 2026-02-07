#pragma once

// Auto-binding marker macros. The shader compiler rewrites these markers into
// explicit register/space declarations. If auto-binding is disabled, these
// expand to plain declarations without register annotations.

#define AE_INTERNAL_SAMPLER_1(name) SamplerState name
#define AE_INTERNAL_SAMPLER_2(type, name) type name
#define AE_INTERNAL_GET_MACRO(_1, _2, NAME, ...) NAME
#define AE_INTERNAL_SAMPLER_DISPATCH(...) \
    AE_INTERNAL_GET_MACRO(__VA_ARGS__, AE_INTERNAL_SAMPLER_2, AE_INTERNAL_SAMPLER_1)(__VA_ARGS__)

#define AE_PER_FRAME_CBUFFER(name) cbuffer name
#define AE_PER_FRAME_SRV(type, name) type name
#define AE_PER_FRAME_UAV(type, name) type name
#define AE_PER_FRAME_SAMPLER(...) AE_INTERNAL_SAMPLER_DISPATCH(__VA_ARGS__)

#define AE_PER_DRAW_CBUFFER(name) cbuffer name
#define AE_PER_DRAW_SRV(type, name) type name
#define AE_PER_DRAW_UAV(type, name) type name
#define AE_PER_DRAW_SAMPLER(...) AE_INTERNAL_SAMPLER_DISPATCH(__VA_ARGS__)

#define AE_PER_MATERIAL_CBUFFER(name) cbuffer name
#define AE_PER_MATERIAL_SRV(type, name) type name
#define AE_PER_MATERIAL_UAV(type, name) type name
#define AE_PER_MATERIAL_SAMPLER(...) AE_INTERNAL_SAMPLER_DISPATCH(__VA_ARGS__)
