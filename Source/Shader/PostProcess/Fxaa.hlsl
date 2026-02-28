// FXAA Quality-style implementation (single pass).
//
// Notes:
// - Uses luma-based edge detection + edge endpoint search for a more stable result than the
//   previous minimal implementation.
// - Parameters keep the same meaning as before (EdgeThreshold / EdgeThresholdMin / Subpix).

#include "Shader/PostProcess/Common.hlsli"

cbuffer FxaaConstants : register(b0)
{
    float EdgeThreshold;
    float EdgeThresholdMin;
    float Subpix;
    float _pad0;
};

float SampleLuma(float2 uv)
{
    return Luma(SceneColor.SampleLevel(LinearSampler, uv, 0.0f).rgb);
}

float3 SampleRgb(float2 uv)
{
    return SceneColor.SampleLevel(LinearSampler, uv, 0.0f).rgb;
}

float4 PSFxaa(FSQOutput input) : SV_Target0
{
    uint w, h;
    SceneColor.GetDimensions(w, h);
    float2 invSize = 1.0f / float2(max(w, 1u), max(h, 1u));

    const float2 uv = input.UV;

    // 3x3 neighborhood (luma only, rgb for center).
    const float3 rgbM = SampleRgb(uv);
    const float  lumaM = Luma(rgbM);

    const float2 offN = float2(0.0f, -invSize.y);
    const float2 offS = float2(0.0f,  invSize.y);
    const float2 offW = float2(-invSize.x, 0.0f);
    const float2 offE = float2( invSize.x, 0.0f);

    const float lumaN  = SampleLuma(uv + offN);
    const float lumaS  = SampleLuma(uv + offS);
    const float lumaW  = SampleLuma(uv + offW);
    const float lumaE  = SampleLuma(uv + offE);
    const float lumaNW = SampleLuma(uv + offN + offW);
    const float lumaNE = SampleLuma(uv + offN + offE);
    const float lumaSW = SampleLuma(uv + offS + offW);
    const float lumaSE = SampleLuma(uv + offS + offE);

    const float lumaMin = min(lumaM, min(min(min(lumaN, lumaS), min(lumaW, lumaE)),
        min(min(lumaNW, lumaNE), min(lumaSW, lumaSE))));
    const float lumaMax = max(lumaM, max(max(max(lumaN, lumaS), max(lumaW, lumaE)),
        max(max(lumaNW, lumaNE), max(lumaSW, lumaSE))));
    const float lumaRange = lumaMax - lumaMin;

    const float edgeThreshold = max(EdgeThresholdMin, lumaMax * EdgeThreshold);
    if (lumaRange < edgeThreshold)
    {
        return float4(rgbM, 1.0f);
    }

    // Estimate whether edge is horizontal or vertical 
    const float edgeH =
        abs(lumaNW - lumaW) + abs(lumaSW - lumaW) +
        abs(lumaNE - lumaE) + abs(lumaSE - lumaE) +
        2.0f * (abs(lumaW - lumaM) + abs(lumaE - lumaM));

    const float edgeV =
        abs(lumaNW - lumaN) + abs(lumaNE - lumaN) +
        abs(lumaSW - lumaS) + abs(lumaSE - lumaS) +
        2.0f * (abs(lumaN - lumaM) + abs(lumaS - lumaM));

    const bool  bHorizontalEdge = (edgeH >= edgeV);
    const float2 stepPerp  = bHorizontalEdge ? float2(0.0f, invSize.y) : float2(invSize.x, 0.0f);
    const float2 stepAlong = bHorizontalEdge ? float2(invSize.x, 0.0f) : float2(0.0f, invSize.y);

    const float lumaNeg = bHorizontalEdge ? lumaN : lumaW; // -stepPerp
    const float lumaPos = bHorizontalEdge ? lumaS : lumaE; // +stepPerp

    const float gradNeg = abs(lumaNeg - lumaM);
    const float gradPos = abs(lumaPos - lumaM);

    const float perpSign = (gradNeg >= gradPos) ? -1.0f : 1.0f;
    const float lumaEdge = 0.5f * (lumaM + ((perpSign < 0.0f) ? lumaNeg : lumaPos));
    const float2 uvEdge = uv + stepPerp * (perpSign * 0.5f);

    // Search along the edge direction to approximate endpoints.
    const float searchThreshold = lumaRange * 0.25f;
    const int   kMaxSearchSteps = 8;

    float distNeg = (float)kMaxSearchSteps;
    float distPos = (float)kMaxSearchSteps;

    [loop]
    for (int step = 1; step <= kMaxSearchSteps; ++step)
    {
        const float2 uvT = uvEdge - stepAlong * (float)step;
        const float  l = SampleLuma(uvT);
        if (abs(l - lumaEdge) >= searchThreshold)
        {
            distNeg = (float)step;
            break;
        }
    }

    [loop]
    for (int step2 = 1; step2 <= kMaxSearchSteps; ++step2)
    {
        const float2 uvT = uvEdge + stepAlong * (float)step2;
        const float  l = SampleLuma(uvT);
        if (abs(l - lumaEdge) >= searchThreshold)
        {
            distPos = (float)step2;
            break;
        }
    }

    const float totalDist = max(distNeg + distPos, 1e-4f);
    const float edgeOffset = 0.5f - (min(distNeg, distPos) / totalDist); // [0..0.5]

    // Sub-pixel aliasing reduction
    const float lumaAvg = 0.25f * (lumaN + lumaS + lumaW + lumaE);
    float subpixBlend = abs(lumaAvg - lumaM) / max(lumaRange, 1e-4f);
    subpixBlend = saturate(subpixBlend);
    subpixBlend *= subpixBlend;
    const float subpixOffset = subpixBlend * Subpix * 0.5f;

    const float finalOffset = max(edgeOffset, subpixOffset);
    float2 uvFinal = uv + stepPerp * (perpSign * finalOffset);

    float3 rgbFinal = SampleRgb(uvFinal);
    const float lumaFinal = Luma(rgbFinal);
    if (lumaFinal < lumaMin || lumaFinal > lumaMax)
    {
        uvFinal = uv + stepPerp * (perpSign * subpixOffset);
        rgbFinal = SampleRgb(uvFinal);
    }

    return float4(rgbFinal, 1.0f);
}
