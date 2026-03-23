#include "Shader/Bindings/ShaderBindings.hlsli"
#include "Shader/Atmosphere/AtmosphereBruneton.hlsli"

AE_PER_FRAME_CBUFFER(AtmosphereParams)
{
    FAtmosphereParams Params;
};

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
    float4   CSM_SplitsVS[4];

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
    FPointLight PointLights[16];
};

AE_PER_FRAME_SRV(Texture2D<float4>, SceneDepth);
AE_PER_FRAME_SRV(Texture2D<float4>, TransmittanceLut);
AE_PER_FRAME_SRV(Texture3D<float4>, ScatteringLut);
AE_PER_FRAME_SRV(Texture3D<float4>, SingleMieScatteringLut);
AE_PER_FRAME_SAMPLER(LinearSampler);

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
    const float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    const float4 clip = float4(ndc, depth, 1.0f);
    float4 world = mul(InvViewProj, clip);
    world.xyz /= max(world.w, 1e-6f);
    return world.xyz;
}

float4 PSAtmosphereSky(FSQOutput input) : SV_Target0
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
    const float3 viewRay = AtmosphereSafeNormalize(world - ViewOriginWS);
    const float3 sunDirection = AtmosphereSafeNormalize(AtmosphereSunDirection(Params));
    const float3 camera = float3(0.0f, AtmosphereViewRadius(Params), 0.0f);

    float3 transmittance = 0.0f.xxx;
    float3 radiance = AtmosphereGetSkyRadiance(Params, TransmittanceLut, ScatteringLut,
        SingleMieScatteringLut, LinearSampler, camera, viewRay, sunDirection, transmittance);

    const float sunCosAngle = dot(viewRay, sunDirection);
    const float sunDisk =
        smoothstep(cos(AtmosphereSunAngularRadiusRad(Params) * 1.5f),
            cos(AtmosphereSunAngularRadiusRad(Params)), sunCosAngle);
    const float3 solarRadiance = AtmosphereSolarIrradiance(Params)
        / max(AE_ATMOSPHERE_PI * AtmosphereSunAngularRadiusRad(Params)
                * AtmosphereSunAngularRadiusRad(Params), 1e-4f);
    radiance += solarRadiance * transmittance * sunDisk;

    return float4(max(radiance * AtmosphereExposure(Params), 0.0f.xxx), 1.0f);
}
