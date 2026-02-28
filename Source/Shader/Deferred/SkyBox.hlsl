// Skybox pass: draw cube map as background (base color only).

#include "Shader/Bindings/ShaderBindings.hlsli"

AE_PER_FRAME_CBUFFER(DeferredView)
{
    // Must match Rendering::FBasicDeferredRenderer::FPerFrameConstants.
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
    float4   CSM_SplitsVS[4];

    float2   RenderTargetSize;
    float2   InvRenderTargetSize;

    uint     bReverseZ;
    uint     DebugShadingMode;
    float    ShadowBias;
    float    _pad0;
    float2   ShadowMapInvSize;
    float2   _pad1;

    // Keep the rest for layout compatibility (not used here).
    struct FPointLight
    {
        float3 PositionWS;
        float  Range;
        float3 Color;
        float  Intensity;
    };
    FPointLight PointLights[16];
};

Texture2D    SceneDepth    : register(t0);
TextureCube  SkyCube       : register(t1);
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
    const float2 ndc  = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    const float4 clip = float4(ndc, depth, 1.0f);
    float4 world      = mul(InvViewProj, clip);
    world.xyz /= max(world.w, 1e-6f);
    return world.xyz;
}

float4 PSSkyBox(FSQOutput input) : SV_Target0
{
    const float depth = SceneDepth.Sample(LinearSampler, input.UV).r;

    if (bReverseZ != 0)
    {
        if (depth > 1e-6f)
        {
            discard;
        }
    }
    else
    {
        if (depth < 1.0f - 1e-6f)
        {
            discard;
        }
    }

    const float3 world = ReconstructWorldPosition(input.UV, depth);
    const float3 dir   = normalize(world - ViewOriginWS);
    const float3 color = SkyCube.Sample(LinearSampler, dir).rgb;
    return float4(color, 1.0f);
}

