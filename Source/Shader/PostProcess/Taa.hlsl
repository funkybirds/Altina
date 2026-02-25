// Temporal Anti-Aliasing (phase 1): matrix + depth reprojection, no velocity buffer.

#define AE_POSTPROCESS_COMMON_NO_DEFAULT_BINDINGS 1
#include "Shader/PostProcess/Common.hlsli"

cbuffer TaaConstants : register(b0)
{
    float2 RenderTargetSize;
    float2 InvRenderTargetSize;

    float2 JitterNdc;
    float2 PrevJitterNdc;

    float  Alpha;
    float  ClampK;
    uint   bHasHistory;
    uint   _pad0;

    row_major float4x4 InvViewProjJittered;
    row_major float4x4 PrevViewProjJittered;
};

Texture2D CurrentColor : register(t0);
Texture2D HistoryColor : register(t1);
Texture2D SceneDepth   : register(t2);
SamplerState LinearSampler : register(s0);

static float3 FetchCurrent(float2 uv)
{
    return CurrentColor.SampleLevel(LinearSampler, uv, 0.0f).rgb;
}

static float FetchDepth(uint2 pixel)
{
    // Depth uses integer fetch to avoid filtering.
    return SceneDepth.Load(int3(pixel, 0)).r;
}

static float3 FetchCurrentPoint(uint2 pixel)
{
    return CurrentColor.Load(int3(pixel, 0)).rgb;
}

static float3 ReconstructWorld(float2 uv, float depth)
{
    // UV is in [0,1] with Y flipped (engine convention).
    const float2 ndc  = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    const float4 clip = float4(ndc, depth, 1.0f);
    float4 world      = mul(InvViewProjJittered, clip);
    world.xyz        /= max(world.w, 1e-6f);
    return world.xyz;
}

static float2 ProjectPrevUv(float3 worldPos)
{
    float4 prevClip = mul(PrevViewProjJittered, float4(worldPos, 1.0f));
    float2 prevNdc  = prevClip.xy / max(prevClip.w, 1e-6f);

    // Convert NDC to UV (Y flipped).
    return float2(prevNdc.x * 0.5f + 0.5f, 0.5f - prevNdc.y * 0.5f);
}

static void NeighborhoodMinMax(uint2 pixel, out float3 outMinC, out float3 outMaxC)
{
    float3 minC = float3( 1e9f,  1e9f,  1e9f);
    float3 maxC = float3(-1e9f, -1e9f, -1e9f);

    [unroll]
    for (int dy = -1; dy <= 1; ++dy)
    {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx)
        {
            int2 p = int2(pixel) + int2(dx, dy);
            p.x = clamp(p.x, 0, int(RenderTargetSize.x) - 1);
            p.y = clamp(p.y, 0, int(RenderTargetSize.y) - 1);

            const float3 c = FetchCurrentPoint(uint2(p));
            minC = min(minC, c);
            maxC = max(maxC, c);
        }
    }

    outMinC = minC;
    outMaxC = maxC;
}

float4 PSTaa(FSQOutput input) : SV_Target0
{
    const float2 uv = input.UV;

    // Convert UV to pixel coordinates for depth/point fetches.
    const float2 pixelF = uv * RenderTargetSize;
    const uint2  pixel  = uint2(clamp(pixelF, float2(0.0f, 0.0f), RenderTargetSize - 1.0f));

    const float  depth = FetchDepth(pixel);
    const float3 curr  = FetchCurrent(uv);

    float historyWeight = Alpha;
    if (bHasHistory == 0)
    {
        historyWeight = 0.0f;
    }

    float3 outColor = curr;
    if (historyWeight > 0.0f)
    {
        const float3 worldPos = ReconstructWorld(uv, depth);
        const float2 prevUv   = ProjectPrevUv(worldPos);

        // Outside history: fallback to current.
        const bool bInside = all(prevUv >= 0.0f) && all(prevUv <= 1.0f);
        if (bInside)
        {
            float3 hist = HistoryColor.SampleLevel(LinearSampler, prevUv, 0.0f).rgb;

            float3 nMin, nMax;
            NeighborhoodMinMax(pixel, nMin, nMax);

            // Clamp history into current neighborhood (optionally tightened by ClampK).
            const float3 lo = lerp(curr, nMin, saturate(ClampK));
            const float3 hi = lerp(curr, nMax, saturate(ClampK));
            hist = clamp(hist, lo, hi);

            outColor = lerp(curr, hist, historyWeight);
        }
    }

    return float4(outColor, 1.0f);
}
