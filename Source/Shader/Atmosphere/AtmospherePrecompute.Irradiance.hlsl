#include "Shader/Atmosphere/AtmosphereBruneton.hlsli"

AE_PER_FRAME_CBUFFER(AtmosphereParams)
{
    FAtmosphereParams Params;
};

AE_PER_FRAME_SRV(Texture2D<float4>, TransmittanceLut);
AE_PER_FRAME_SAMPLER(LinearSampler);
AE_PER_FRAME_UAV(RWTexture2D<float4>, OutIrradiance);

[numthreads(16, 16, 1)]
void CSAtmosphereIrradiance(uint3 dispatchThreadId : SV_DispatchThreadID)
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
    const float3 irradiance =
        AtmosphereComputeDirectIrradiance(Params, TransmittanceLut, LinearSampler, r, muS);
    OutIrradiance[dispatchThreadId.xy] = float4(irradiance, 1.0f);
}
