// Bloom: prefilter + downsample + separable Gaussian blur (horizontal/vertical).

#include "Shader/PostProcess/Common.hlsli"

cbuffer BloomConstants : register(b0)
{
    float Threshold;
    float Knee;
    float Intensity;
    // Historically named "KawaseOffset". It is now used as a blur radius (in pixels) scaling the
    // Gaussian kernel tap offsets. 0 disables blur.
    float KawaseOffset;
};

float3 PrefilterBloom(float3 hdr)
{
    float threshold = max(Threshold, 0.0f);
    float knee      = max(Knee, 0.0f);

    float luma = Luma(hdr);
    float soft = luma - threshold;
    if (knee > 1e-6f)
    {
        soft = saturate(soft / knee);
        soft = soft * soft;
    }
    else
    {
        soft = (soft > 0.0f) ? 1.0f : 0.0f;
    }

    float scale = (luma > 1e-6f) ? (max(luma - threshold, 0.0f) / luma) : 0.0f;
    scale = max(scale, soft);
    return hdr * scale;
}

float3 DownsampleBox2x2(Texture2D tex, SamplerState samp, float2 uv, float2 invSize)
{
    float2 o = 0.5f * invSize;
    float3 c0 = tex.Sample(samp, uv + float2(-o.x, -o.y)).rgb;
    float3 c1 = tex.Sample(samp, uv + float2( o.x, -o.y)).rgb;
    float3 c2 = tex.Sample(samp, uv + float2(-o.x,  o.y)).rgb;
    float3 c3 = tex.Sample(samp, uv + float2( o.x,  o.y)).rgb;
    return 0.25f * (c0 + c1 + c2 + c3);
}

float3 DownsampleBox2x2LumaWeighted(Texture2D tex, SamplerState samp, float2 uv, float2 invSize)
{
    float2 o = 0.5f * invSize;
    float3 c0 = tex.Sample(samp, uv + float2(-o.x, -o.y)).rgb;
    float3 c1 = tex.Sample(samp, uv + float2( o.x, -o.y)).rgb;
    float3 c2 = tex.Sample(samp, uv + float2(-o.x,  o.y)).rgb;
    float3 c3 = tex.Sample(samp, uv + float2( o.x,  o.y)).rgb;

    float w0 = rcp(1.0f + Luma(c0));
    float w1 = rcp(1.0f + Luma(c1));
    float w2 = rcp(1.0f + Luma(c2));
    float w3 = rcp(1.0f + Luma(c3));

    float3 sum = c0 * w0 + c1 * w1 + c2 * w2 + c3 * w3;
    float  w   = max(w0 + w1 + w2 + w3, 1e-6f);
    return sum / w;
}

float3 GaussianBlur5Tap1D(Texture2D tex, SamplerState samp, float2 uv, float2 invSize, float2 axis)
{
    // sigma ~= 2.0 -> [1, 0.8825, 0.6065] normalized.
    const float w0 = 0.251379f;
    const float w1 = 0.221841f;
    const float w2 = 0.152469f;

    const float radiusPx = max(KawaseOffset, 0.0f);
    const float2 step1 = axis * invSize * radiusPx;
    const float2 step2 = step1 * 2.0f;

    float3 c = tex.Sample(samp, uv).rgb * w0;

    c += (tex.Sample(samp, uv + step1).rgb + tex.Sample(samp, uv - step1).rgb) * w1;
    c += (tex.Sample(samp, uv + step2).rgb + tex.Sample(samp, uv - step2).rgb) * w2;
    return c;
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
    float3 c = DownsampleBox2x2(SceneColor, LinearSampler, input.UV, invSize);
    return float4(c, 1.0f);
}

float4 PSBloomDownsampleWeighted(FSQOutput input) : SV_Target0
{
    uint w, h;
    SceneColor.GetDimensions(w, h);
    float2 invSize = 1.0f / float2(max(w, 1u), max(h, 1u));
    float3 c = DownsampleBox2x2LumaWeighted(SceneColor, LinearSampler, input.UV, invSize);
    return float4(c, 1.0f);
}

float4 PSBloomBlurH(FSQOutput input) : SV_Target0
{
    uint w, h;
    SceneColor.GetDimensions(w, h);
    float2 invSize = 1.0f / float2(max(w, 1u), max(h, 1u));
    float3 c = GaussianBlur5Tap1D(SceneColor, LinearSampler, input.UV, invSize, float2(1.0f, 0.0f));
    return float4(c, 1.0f);
}

float4 PSBloomBlurV(FSQOutput input) : SV_Target0
{
    uint w, h;
    SceneColor.GetDimensions(w, h);
    float2 invSize = 1.0f / float2(max(w, 1u), max(h, 1u));
    float3 c = GaussianBlur5Tap1D(SceneColor, LinearSampler, input.UV, invSize, float2(0.0f, 1.0f));
    return float4(c, 1.0f);
}

float4 PSBloomUpsample(FSQOutput input) : SV_Target0
{
    float3 c = SceneColor.Sample(LinearSampler, input.UV).rgb;
    return float4(c, 1.0f);
}

float4 PSBloomApply(FSQOutput input) : SV_Target0
{
    float3 bloom = SceneColor.Sample(LinearSampler, input.UV).rgb;
    bloom *= max(Intensity, 0.0f);
    return float4(bloom, 1.0f);
}
