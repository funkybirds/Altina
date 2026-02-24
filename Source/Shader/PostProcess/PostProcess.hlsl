// Built-in post-process shaders (Phase1): Blit / Tonemap / FXAA.

cbuffer PostProcessConstants : register(b0)
{
    float Exposure;
    float Gamma;
    float FxaaEdgeThreshold;
    float FxaaEdgeThresholdMin;
    float FxaaSubpix;
    float _pad0;
    float _pad1;
    float _pad2;
};

Texture2D    SceneColor : register(t0);
SamplerState LinearSampler : register(s0);

struct FSQOutput
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

FSQOutput VSFullScreenTriangle(uint vertexId : SV_VertexID)
{
    FSQOutput output;
    float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };
    float2 uvs[3] =
    {
        // Keep the engine convention used by DeferredLighting: UV.y is flipped.
        float2(0.0f, 1.0f),
        float2(0.0f, -1.0f),
        float2(2.0f, 1.0f)
    };
    output.Position = float4(positions[vertexId], 0.0f, 1.0f);
    output.UV       = uvs[vertexId];
    return output;
}

float3 TonemapReinhard(float3 x)
{
    return x / (1.0f + x);
}

float3 LinearToGamma(float3 x, float gamma)
{
    // gamma should be > 0
    float invGamma = (gamma > 1e-6f) ? (1.0f / gamma) : 1.0f;
    return pow(saturate(x), invGamma);
}

float4 PSBlit(FSQOutput input) : SV_Target0
{
    return SceneColor.Sample(LinearSampler, input.UV);
}

float4 PSTonemap(FSQOutput input) : SV_Target0
{
    float3 hdr = SceneColor.Sample(LinearSampler, input.UV).rgb;
    hdr *= max(Exposure, 0.0f);
    float3 ldr = TonemapReinhard(hdr);

    // BackBuffer is Unorm (not SRGB) by default in D3D11 viewport, so do gamma here.
    ldr = LinearToGamma(ldr, max(Gamma, 1e-6f));
    return float4(ldr, 1.0f);
}

// Minimal FXAA (based on FXAA 3.x style, simplified).
float Luma(float3 rgb)
{
    return dot(rgb, float3(0.299f, 0.587f, 0.114f));
}

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

    const float edgeThreshold = max(FxaaEdgeThresholdMin, lumaMax * FxaaEdgeThreshold);
    if (lumaRange < edgeThreshold)
    {
        return float4(rgbM, 1.0f);
    }

    float2 dir;
    dir.x = -((lumaN + lumaS) - 2.0f * lumaM);
    dir.y =  ((lumaW + lumaE) - 2.0f * lumaM);

    const float dirReduce = max((lumaN + lumaS + lumaW + lumaE) * (0.25f * FxaaSubpix), 1e-4f);
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

