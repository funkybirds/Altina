#include "Shader/Atmosphere/AtmosphereBruneton.hlsli"

AE_PER_FRAME_CBUFFER(AtmosphereParams)
{
    FAtmosphereParams Params;
};

AE_PER_FRAME_SRV(Texture3D<float4>, SingleRayleighScatteringLut);
AE_PER_FRAME_SRV(Texture3D<float4>, SingleMieScatteringLut);
AE_PER_FRAME_SRV(Texture3D<float4>, MultipleScatteringLut);
AE_PER_FRAME_SRV(Texture2D<float4>, IrradianceLut);
AE_PER_FRAME_SAMPLER(LinearSampler);
AE_PER_FRAME_UAV(RWTexture2D<float4>, OutDeltaIrradiance);
AE_PER_FRAME_UAV(RWTexture2D<float4>, OutIrradiance);

[numthreads(16, 16, 1)]
void CSAtmosphereIndirectIrradiance(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint width;
    uint height;
    OutIrradiance.GetDimensions(width, height);
    if (dispatchThreadId.x >= width || dispatchThreadId.y >= height)
    {
        return;
    }

    float r = 0.0f;
    float muS = 0.0f;
    const float2 uv = (float2(dispatchThreadId.xy) + 0.5f) / float2(width, height);
    AtmosphereGetRMuSFromIrradianceUv(Params, uv, r, muS);

    const float3 deltaIrradiance = AtmosphereComputeIndirectIrradiance(Params,
        SingleRayleighScatteringLut, SingleMieScatteringLut, MultipleScatteringLut,
        LinearSampler, r, muS, AtmosphereScatteringOrder(Params) - 1u);
    const float3 oldIrradiance = OutIrradiance[dispatchThreadId.xy].rgb;
    OutDeltaIrradiance[dispatchThreadId.xy] = float4(deltaIrradiance, 0.0f);
    OutIrradiance[dispatchThreadId.xy] = float4(oldIrradiance + deltaIrradiance, 1.0f);
}
