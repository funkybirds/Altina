#include "Shader/Atmosphere/AtmosphereBruneton.hlsli"

AE_PER_FRAME_CBUFFER(AtmosphereParams)
{
    FAtmosphereParams Params;
};

AE_PER_FRAME_UAV(RWTexture2D<float4>, OutTransmittance);

[numthreads(16, 16, 1)]
void CSAtmosphereTransmittance(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint width;
    uint height;
    OutTransmittance.GetDimensions(width, height);
    if (dispatchThreadId.x >= width || dispatchThreadId.y >= height)
    {
        return;
    }

    float r = 0.0f;
    float mu = 0.0f;
    const float2 uv = (float2(dispatchThreadId.xy) + 0.5f) / float2(width, height);
    AtmosphereGetRMuFromTransmittanceUv(Params, uv, r, mu);
    const float3 transmittance = AtmosphereComputeTransmittanceToTopBoundary(Params, r, mu);
    OutTransmittance[dispatchThreadId.xy] = float4(transmittance, 1.0f);
}
