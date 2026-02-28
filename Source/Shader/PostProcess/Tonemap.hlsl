// Minimal tonemap pass (HDR -> LDR).

#include "Shader/PostProcess/Common.hlsli"

cbuffer TonemapConstants : register(b0)
{
    float Exposure;
    float Gamma;
    float _pad0;
    float _pad1;
};

float3 TonemapReinhard(float3 x)
{
    return x / (1.0f + x);
}

// ACES filmic tonemap approximation (Narkowicz 2015 fit).
float3 TonemapACESFitted(float3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 PSTonemap(FSQOutput input) : SV_Target0
{
    float3 hdr = SceneColor.Sample(LinearSampler, input.UV).rgb;
    hdr *= max(Exposure, 0.0f);
    float3 ldr = TonemapACESFitted(hdr);

    // Require refactor: shader perm for sRGB swapchain
    // BackBuffer is Unorm (not SRGB) by default in D3D11 viewport, so do gamma here.
    ldr = LinearToGamma(ldr, max(Gamma, 1e-6f));
    return float4(ldr, 1.0f);
}
