// Minimal FXAA (based on FXAA 3.x style, simplified).

#include "Shader/PostProcess/Common.hlsli"

cbuffer FxaaConstants : register(b0)
{
    float EdgeThreshold;
    float EdgeThresholdMin;
    float Subpix;
    float _pad0;
};

float4 PSFxaa(FSQOutput input) : SV_Target0
{
    uint w, h;
    SceneColor.GetDimensions(w, h);
    float2 invSize = 1.0f / float2(max(w, 1u), max(h, 1u));

    const float2 uv = input.UV;
    const float3 rgbM = SceneColor.Sample(LinearSampler, uv).rgb;
    const float3 rgbN = SceneColor.Sample(LinearSampler, uv + float2(0.0f, -invSize.y)).rgb;
    const float3 rgbS = SceneColor.Sample(LinearSampler, uv + float2(0.0f,  invSize.y)).rgb;
    const float3 rgbW = SceneColor.Sample(LinearSampler, uv + float2(-invSize.x, 0.0f)).rgb;
    const float3 rgbE = SceneColor.Sample(LinearSampler, uv + float2( invSize.x, 0.0f)).rgb;

    const float lumaM = Luma(rgbM);
    const float lumaN = Luma(rgbN);
    const float lumaS = Luma(rgbS);
    const float lumaW = Luma(rgbW);
    const float lumaE = Luma(rgbE);

    const float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
    const float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
    const float lumaRange = lumaMax - lumaMin;

    const float edgeThreshold = max(EdgeThresholdMin, lumaMax * EdgeThreshold);
    if (lumaRange < edgeThreshold)
    {
        return float4(rgbM, 1.0f);
    }

    float2 dir;
    dir.x = -((lumaN + lumaS) - 2.0f * lumaM);
    dir.y =  ((lumaW + lumaE) - 2.0f * lumaM);

    const float dirReduce = max((lumaN + lumaS + lumaW + lumaE) * (0.25f * Subpix), 1e-4f);
    const float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, -8.0f, 8.0f) * invSize;

    const float3 rgbA =
        0.5f * (SceneColor.Sample(LinearSampler, uv + dir * (1.0f / 3.0f - 0.5f)).rgb +
                SceneColor.Sample(LinearSampler, uv + dir * (2.0f / 3.0f - 0.5f)).rgb);
    const float3 rgbB =
        rgbA * 0.5f +
        0.25f * (SceneColor.Sample(LinearSampler, uv + dir * -0.5f).rgb +
                SceneColor.Sample(LinearSampler, uv + dir * 0.5f).rgb);

    const float lumaB = Luma(rgbB);
    if (lumaB < lumaMin || lumaB > lumaMax)
    {
        return float4(rgbA, 1.0f);
    }
    return float4(rgbB, 1.0f);
}
