// Bloom: prefilter + downsample/upsample using a simple Kawase-like blur.

#include "Shader/PostProcess/Common.hlsli"

cbuffer BloomConstants : register(b0)
{
    float Threshold;
    float Knee;
    float Intensity;
    float KawaseOffset;
};

float3 PrefilterBloom(float3 hdr)
{
    float threshold = max(Threshold, 0.0f);
    float knee      = max(Knee, 0.0f);

    // Luminance-based soft threshold.
    float luma = Luma(hdr);
    float soft = luma - threshold;
    if (knee > 1e-6f)
    {
        soft = saturate(soft / knee);
        soft = soft * soft; // smoother ramp
    }
    else
    {
        soft = (soft > 0.0f) ? 1.0f : 0.0f;
    }

    float scale = (luma > 1e-6f) ? (max(luma - threshold, 0.0f) / luma) : 0.0f;
    scale = max(scale, soft);
    return hdr * scale;
}

float3 KawaseBlur(Texture2D tex, SamplerState samp, float2 uv, float2 invSize, float offsetPx)
{
    float2 o = invSize * offsetPx;
    float3 c0 = tex.Sample(samp, uv + float2(-o.x, -o.y)).rgb;
    float3 c1 = tex.Sample(samp, uv + float2( o.x, -o.y)).rgb;
    float3 c2 = tex.Sample(samp, uv + float2(-o.x,  o.y)).rgb;
    float3 c3 = tex.Sample(samp, uv + float2( o.x,  o.y)).rgb;
    return 0.25f * (c0 + c1 + c2 + c3);
}

float4 PSBloomPrefilter(FSQOutput input) : SV_Target0
{
    float3 hdr = SceneColor.Sample(LinearSampler, input.UV).rgb;
    return float4(PrefilterBloom(hdr), 1.0f);
}

float4 PSBloomDownsample(FSQOutput input) : SV_Target0
{
    uint w, h;
    SceneColor.GetDimensions(w, h);
    float2 invSize = 1.0f / float2(max(w, 1u), max(h, 1u));
    float offsetPx = max(KawaseOffset, 0.0f) + 0.5f;
    float3 blurred = KawaseBlur(SceneColor, LinearSampler, input.UV, invSize, offsetPx);
    return float4(blurred, 1.0f);
}

float4 PSBloomUpsample(FSQOutput input) : SV_Target0
{
    uint w, h;
    SceneColor.GetDimensions(w, h);
    float2 invSize = 1.0f / float2(max(w, 1u), max(h, 1u));
    float offsetPx = max(KawaseOffset, 0.0f) + 0.5f;
    float3 blurred = KawaseBlur(SceneColor, LinearSampler, input.UV, invSize, offsetPx);
    return float4(blurred, 1.0f);
}

float4 PSBloomApply(FSQOutput input) : SV_Target0
{
    float3 bloom = SceneColor.Sample(LinearSampler, input.UV).rgb;
    bloom *= max(Intensity, 0.0f);
    return float4(bloom, 1.0f);
}
