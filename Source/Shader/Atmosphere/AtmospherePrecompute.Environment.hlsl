#include "Shader/Atmosphere/AtmosphereCommon.hlsli"

AE_PER_FRAME_CBUFFER(AtmosphereParams)
{
    FAtmosphereParams Params;
};

AE_PER_FRAME_SRV(Texture2D<float4>, TransmittanceLut);
AE_PER_FRAME_SRV(Texture2D<float4>, MultiScatterLut);
AE_PER_FRAME_UAV(RWTexture2DArray<float4>, OutEnvironment);
AE_PER_FRAME_SAMPLER(LinearSampler);

[numthreads(8, 8, 1)]
void CSAtmosphereEnvironment(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint width;
    uint height;
    uint layers;
    OutEnvironment.GetDimensions(width, height, layers);
    if (dispatchThreadId.x >= width || dispatchThreadId.y >= height || dispatchThreadId.z >= layers)
    {
        return;
    }

    const float2 uv = (float2(dispatchThreadId.xy) + 0.5f) / float2(width, height);
    const float2 faceUv = uv * 2.0f - 1.0f;
    const uint   faceIndex = dispatchThreadId.z;
    const float3 direction = AtmosphereFaceUvToDirection(faceIndex, faceUv);

    float roughness = AtmosphereRoughness(Params);
    if (AtmosphereOutputMode(Params) == AE_ATMOSPHERE_OUTPUT_IRRADIANCE)
    {
        roughness = 0.7f;
    }

    const float2 lutUv = float2(uv.x, saturate(direction.y * 0.5f + 0.5f));
    const float3 transmittance   = TransmittanceLut.SampleLevel(LinearSampler, lutUv, 0.0f).rgb;
    const float3 multiScattering = MultiScatterLut.SampleLevel(LinearSampler, lutUv, 0.0f).rgb;
    float3 color = ComputeAtmosphereColor(Params, direction, roughness);
    color *= lerp(0.7f.xxx + multiScattering, 1.0f.xxx + multiScattering, transmittance);
    OutEnvironment[dispatchThreadId] = float4(color, 1.0f);
}
