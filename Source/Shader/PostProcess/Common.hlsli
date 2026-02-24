// Common definitions for post-process shaders.
// Keep this file small and reusable across effects (Blit / Tonemap / FXAA / ...).

#ifndef AE_POSTPROCESS_COMMON_HLSLI
#define AE_POSTPROCESS_COMMON_HLSLI 1

Texture2D    SceneColor : register(t0);
SamplerState LinearSampler : register(s0);

struct FSQOutput
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

float3 LinearToGamma(float3 x, float gamma)
{
    // gamma should be > 0
    float invGamma = (gamma > 1e-6f) ? (1.0f / gamma) : 1.0f;
    return pow(saturate(x), invGamma);
}

float Luma(float3 rgb)
{
    return dot(rgb, float3(0.299f, 0.587f, 0.114f));
}

#endif // AE_POSTPROCESS_COMMON_HLSLI
