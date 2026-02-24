// Deferred lighting + optional directional CSM shadows (Phase1).

#include "Shader/Bindings/ShaderBindings.hlsli"

#define AE_PBR_STANDARD_NO_CBUFFERS
#include "Shader/Deferred/Materials/Lit/PBR.Standard.hlsl"
#undef AE_PBR_STANDARD_NO_CBUFFERS

#define AE_MAX_POINT_LIGHTS 16

AE_PER_FRAME_CBUFFER(DeferredView)
{
    // IMPORTANT:
    // This cbuffer is backed by the engine's shared `FPerFrameConstants` which also feeds the
    // base pass / shadow pass view constants. `ViewProjection` must remain the first member to
    // keep the layout compatible across shaders.
    row_major float4x4 ViewProjection;

    row_major float4x4 View;
    row_major float4x4 Proj;
    row_major float4x4 ViewProj;
    row_major float4x4 InvViewProj;

    float3   ViewOriginWS;
    uint     PointLightCount;

    float3   DirLightDirectionWS;
    float    DirLightIntensity;
    float3   DirLightColor;
    uint     CSMCascadeCount;

    row_major float4x4 CSM_LightViewProj[4];
    float4   CSM_SplitsVS[4]; // x=near, y=far

    float2   RenderTargetSize;
    float2   InvRenderTargetSize;

    uint     bReverseZ;
    uint     DebugShadingMode; // 0=PBR, 1=Lambert (debug)
    float    ShadowBias;
    float    _pad0;
    float2   ShadowMapInvSize;
    float2   _pad1;

    struct FPointLight
    {
        float3 PositionWS;
        float  Range;
        float3 Color;
        float  Intensity;
    };
    FPointLight PointLights[AE_MAX_POINT_LIGHTS];
};

Texture2D      GBufferA   : register(t0);
Texture2D      GBufferB   : register(t1);
Texture2D      GBufferC   : register(t2);
Texture2D      SceneDepth : register(t3);
Texture2DArray ShadowMap  : register(t4);
SamplerState   LinearSampler : register(s0);

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
        float2(0.0f, 1.0f),
        float2(0.0f, -1.0f),
        float2(2.0f, 1.0f)
    };
    output.Position = float4(positions[vertexId], 0.0f, 1.0f);
    output.UV       = uvs[vertexId];
    return output;
}

float3 ReconstructWorldPosition(float2 uv, float depth)
{
    // UV is in [0,1] with Y flipped (matches BasicDeferred full-screen triangle convention).
    const float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    const float4 clip = float4(ndc, depth, 1.0f);
    float4 world = mul(InvViewProj, clip);
    world.xyz /= max(world.w, 1e-6f);
    return world.xyz;
}

float CompareShadowDepth(float receiverDepth, float storedDepth)
{
    // Returns 1 if lit, 0 if shadowed.
    if (bReverseZ != 0)
    {
        // Reverse-Z: larger depth is closer.
        return (receiverDepth >= storedDepth) ? 1.0f : 0.0f;
    }
    return (receiverDepth <= storedDepth) ? 1.0f : 0.0f;
}

float SampleShadowPCF(uint cascadeIndex, float3 positionWS)
{
    if (CSMCascadeCount == 0 || cascadeIndex >= CSMCascadeCount)
    {
        return 1.0f;
    }

    const float4 shadowPos = mul(CSM_LightViewProj[cascadeIndex], float4(positionWS, 1.0f));
    const float2 uv = shadowPos.xy / max(shadowPos.w, 1e-6f) * 0.5f + 0.5f;
    float depth = shadowPos.z / max(shadowPos.w, 1e-6f);

    // Receiver bias in NDC depth.
    if (bReverseZ != 0)
    {
        depth -= ShadowBias;
    }
    else
    {
        depth += ShadowBias;
    }

    // Outside the atlas slice -> treat as lit.
    if (any(uv < 0.0f) || any(uv > 1.0f))
    {
        return 1.0f;
    }

    const float2 texel = ShadowMapInvSize;
    float sum = 0.0f;
    // 2x2 PCF
    sum += CompareShadowDepth(depth, ShadowMap.Sample(LinearSampler, float3(uv + texel * float2(-0.5f, -0.5f), cascadeIndex)).r);
    sum += CompareShadowDepth(depth, ShadowMap.Sample(LinearSampler, float3(uv + texel * float2( 0.5f, -0.5f), cascadeIndex)).r);
    sum += CompareShadowDepth(depth, ShadowMap.Sample(LinearSampler, float3(uv + texel * float2(-0.5f,  0.5f), cascadeIndex)).r);
    sum += CompareShadowDepth(depth, ShadowMap.Sample(LinearSampler, float3(uv + texel * float2( 0.5f,  0.5f), cascadeIndex)).r);
    return sum * 0.25f;
}

uint SelectCascade(float viewDepthVS)
{
    if (CSMCascadeCount == 0)
    {
        return 0;
    }

    // Avoid `break` inside `[unroll]` loops: fxc can fail to unroll such loops.
    uint cascade = 0;
    //[unroll(4)]
    for (uint i = 0; i < 4; ++i)
    {
        if (i >= CSMCascadeCount)
        {
            continue;
        }

        // If we're beyond the cascade far plane, advance the cascade index.
        if (viewDepthVS > CSM_SplitsVS[i].y)
        {
            cascade = min(i + 1, CSMCascadeCount - 1);
        }
    }
    return cascade;
}

float4 PSDeferredLighting(FSQOutput input) : SV_Target0
{
    const float2 uv = input.UV;

    const float4 gbufferA = GBufferA.Sample(LinearSampler, uv);
    const float4 gbufferB = GBufferB.Sample(LinearSampler, uv);
    const float4 gbufferC = GBufferC.Sample(LinearSampler, uv);
    const FPbrGBufferData data = DecodePbrGBuffer(gbufferA, gbufferB, gbufferC);

    const float depth = SceneDepth.Sample(LinearSampler, uv).r;
    // Reverse-Z clears depth to 0 (far). Treat that as background.
    if (depth <= 1e-6f)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    const float3 positionWS = ReconstructWorldPosition(uv, depth);
    const float4 positionVS4 = mul(View, float4(positionWS, 1.0f));

    // View vector in world space.
    float3 V = ViewOriginWS - positionWS;
    const float vLen2 = dot(V, V);
    V = (vLen2 > 1e-8f) ? (V * rsqrt(vLen2)) : float3(0.0f, 0.0f, 1.0f);
    const float3 N = normalize(data.Normal);

    // Cascade selection uses view-space depth (positive forward).
    const float viewDepthVS = positionVS4.z;
    const uint cascadeIndex = SelectCascade(viewDepthVS);
    const float shadow = SampleShadowPCF(cascadeIndex, positionWS);

    float3 color = data.Emissive;

    // Simple ambient term.
    color += data.BaseColor * 0.02f;

    // Main directional.
    const float3 Ld = normalize(-DirLightDirectionWS);
    float3 dirLight = 0.0f;
    if (DebugShadingMode != 0)
    {
        // Lambert diffuse for debugging material/lighting correctness without PBR inputs.
        const float NdotL = saturate(dot(N, Ld));
        dirLight = data.BaseColor * (DirLightColor * DirLightIntensity) * NdotL;
    }
    else
    {
        dirLight = EvaluatePbrDirect(data, N, V, Ld, DirLightColor * DirLightIntensity);
    }
    color += dirLight * shadow;

    // Points.
    const uint count = min(PointLightCount, (uint)AE_MAX_POINT_LIGHTS);
    [loop]
    for (uint i = 0; i < count; ++i)
    {
        const float3 toL = PointLights[i].PositionWS - positionWS;
        const float dist = length(toL);
        const float range = max(PointLights[i].Range, 1e-3f);
        if (dist >= range)
        {
            continue;
        }
        const float3 L = toL / max(dist, 1e-3f);
        const float att = saturate(1.0f - dist / range);
        const float intensity = PointLights[i].Intensity * (att * att);
        if (DebugShadingMode != 0)
        {
            const float NdotL = saturate(dot(N, L));
            color += data.BaseColor * (PointLights[i].Color * intensity) * NdotL;
        }
        else
        {
            color += EvaluatePbrDirect(data, N, V, L, PointLights[i].Color * intensity);
        }
    }

    // AO (very simple: darken everything a bit).
    color *= lerp(0.35f, 1.0f, data.Occlusion);

    return float4(color, 1.0f);
}
