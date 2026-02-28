// SSAO

#include "Shader/Bindings/ShaderBindings.hlsli"

#define AE_MAX_POINT_LIGHTS 16

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

// Keep the layout identical to DeferredLighting.hlsl 
AE_PER_FRAME_CBUFFER(DeferredView)
{
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

    row_major float4x4 CSM_LightViewProj0;
    row_major float4x4 CSM_LightViewProj1;
    row_major float4x4 CSM_LightViewProj2;
    row_major float4x4 CSM_LightViewProj3;
    float4   CSM_SplitsVS[4]; // x=near, y=far

    float2   RenderTargetSize;
    float2   InvRenderTargetSize;

    uint     bReverseZ;
    uint     DebugShadingMode;
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

cbuffer SsaoConstants : register(b1)
{
    // 0 = disabled (outputs 1.0), 1 = enabled.
    uint  Enable;
    uint  SampleCount;
    float RadiusVS;
    float BiasNdc;

    float Power;
    float Intensity;
    float _padSsao0;
    float _padSsao1;
};

// Inputs.
Texture2D    GBufferB   : register(t0); // normal.xyz in [0,1], roughness in a
Texture2D    SceneDepth : register(t1); // NDC depth in [0,1] (reverse-Z supported)
SamplerState LinearSampler : register(s0);

static const uint kMaxSamples = 16u;
static const float3 kKernel[kMaxSamples] =
{
    float3( 0.5381f,  0.1856f, 0.4319f),
    float3( 0.1379f,  0.2486f, 0.4430f),
    float3( 0.3371f,  0.5679f, 0.0057f),
    float3(-0.6999f, -0.0451f, 0.0019f),
    float3( 0.0689f, -0.1598f, 0.8547f),
    float3( 0.0560f,  0.0069f, 0.1843f),
    float3(-0.0146f,  0.1402f, 0.0762f),
    float3( 0.0100f, -0.1924f, 0.0344f),
    float3(-0.3577f, -0.5301f, 0.4358f),
    float3(-0.3169f,  0.1063f, 0.0158f),
    float3( 0.0103f, -0.5869f, 0.0046f),
    float3(-0.0897f, -0.4940f, 0.3287f),
    float3( 0.7119f, -0.0154f, 0.0918f),
    float3(-0.0533f,  0.0596f, 0.5411f),
    float3( 0.0352f, -0.0631f, 0.5460f),
    float3(-0.4776f,  0.2847f, 0.0271f),
};

float3 DecodeNormalWS(float4 gbufferB)
{
    return normalize(gbufferB.xyz * 2.0f - 1.0f);
}

float3 ReconstructWorldPosition(float2 uv, float depth)
{
    // UV is in [0,1] with Y flipped (matches DeferredLighting convention).
    const float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    const float4 clip = float4(ndc, depth, 1.0f);
    float4 world = mul(InvViewProj, clip);
    world.xyz /= max(world.w, 1e-6f);
    return world.xyz;
}

float2 ProjectUvFromClip(float4 clip)
{
    const float invW = 1.0f / max(clip.w, 1e-6f);
    float2 ndc = clip.xy * invW;
    float2 uv = ndc * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;
    return uv;
}

bool IsBackgroundDepth(float depth)
{
    if (bReverseZ != 0)
    {
        return depth <= 1e-6f;
    }
    return depth >= (1.0f - 1e-6f);
}

float OccluderCloserThanSample(float occluderDepthNdc, float sampleDepthNdc)
{
    if (bReverseZ != 0)
    {
        return (occluderDepthNdc > sampleDepthNdc) ? 1.0f : 0.0f;
    }
    return (occluderDepthNdc < sampleDepthNdc) ? 1.0f : 0.0f;
}

float Hash01(uint2 p)
{
    uint h = p.x * 1664525u + p.y * 1013904223u + 374761393u;
    h ^= (h >> 16);
    h *= 2246822519u;
    h ^= (h >> 13);
    h *= 3266489917u;
    return (h & 0x00ffffffu) / 16777216.0f;
}

float PSSsao(FSQOutput input) : SV_Target0
{
    if (Enable == 0u || Intensity <= 1e-6f)
    {
        return 1.0f;
    }

    const float2 uv = input.UV;

    const float depth = SceneDepth.Sample(LinearSampler, uv).r;
    if (IsBackgroundDepth(depth))
    {
        return 1.0f;
    }

    const float3 posWS = ReconstructWorldPosition(uv, depth);
    const float3 posVS = mul(View, float4(posWS, 1.0f)).xyz;

    const float4 gbB = GBufferB.Sample(LinearSampler, uv);
    const float3 nWS = DecodeNormalWS(gbB);
    float3 nVS = mul((float3x3)View, nWS);
    nVS = normalize(nVS);

    const uint2 pix = (uint2)input.Position.xy;
    const float r0 = Hash01(pix);
    const float r1 = Hash01(pix.yx + 17u);
    float3 randDir = normalize(float3(r0 * 2.0f - 1.0f, r1 * 2.0f - 1.0f, 0.0f));
    float3 T = randDir - nVS * dot(randDir, nVS);
    const float tLen2 = dot(T, T);
    T = (tLen2 > 1e-6f) ? (T * rsqrt(tLen2)) : float3(1.0f, 0.0f, 0.0f);
    const float3 B = normalize(cross(nVS, T));

    const uint count = min(SampleCount, kMaxSamples);
    const float radius = max(RadiusVS, 1e-4f);
    const float bias = max(BiasNdc, 0.0f);

    float occ = 0.0f;
    [loop]
    for (uint i = 0u; i < count; ++i)
    {
        const float3 k = kKernel[i];
        const float3 sampleDirVS = normalize(T * k.x + B * k.y + nVS * max(k.z, 0.0f));
        const float3 samplePosVS = posVS + sampleDirVS * radius;

        const float4 sampleClip = mul(Proj, float4(samplePosVS, 1.0f));
        const float2 sampleUv = ProjectUvFromClip(sampleClip);

        const float inRange =
            step(0.0f, sampleUv.x) * step(0.0f, sampleUv.y) * step(sampleUv.x, 1.0f)
            * step(sampleUv.y, 1.0f);
        if (inRange <= 0.0f)
        {
            continue;
        }

        float sampleDepthNdc = sampleClip.z / max(sampleClip.w, 1e-6f);
        if (bReverseZ != 0)
        {
            sampleDepthNdc += bias;
        }
        else
        {
            sampleDepthNdc -= bias;
        }
        const float occluderDepthNdc = SceneDepth.SampleLevel(LinearSampler, sampleUv, 0.0f).r;

        const float closer = OccluderCloserThanSample(occluderDepthNdc, sampleDepthNdc);
        const float dz = abs(posVS.z - samplePosVS.z);
        const float rangeCheck = smoothstep(0.0f, 1.0f, radius / max(dz, 1e-3f));
        occ += closer * rangeCheck;
    }

    float ao = 1.0f - (occ / max((float)count, 1.0f));
    ao = saturate(ao);

    ao = pow(ao, max(Power, 0.1f));
    ao = lerp(1.0f, ao, saturate(Intensity));
    return ao;
}
