#include "Shader/Atmosphere/AtmosphereBruneton.hlsli"

AE_PER_FRAME_CBUFFER(AtmosphereParams)
{
    FAtmosphereParams Params;
};

AE_PER_FRAME_SRV(Texture2D<float4>, TransmittanceLut);
AE_PER_FRAME_SAMPLER(LinearSampler);
AE_PER_FRAME_UAV(RWTexture3D<float4>, OutSingleRayleighScattering);
AE_PER_FRAME_UAV(RWTexture3D<float4>, OutScattering);
AE_PER_FRAME_UAV(RWTexture3D<float4>, OutSingleMieScattering);

[numthreads(16, 16, 1)]
void CSAtmosphereSingleScattering(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint width;
    uint height;
    uint depth;
    OutScattering.GetDimensions(width, height, depth);
    if (dispatchThreadId.x >= width || dispatchThreadId.y >= height || dispatchThreadId.z >= depth)
    {
        return;
    }

    float r = 0.0f;
    float mu = 0.0f;
    float muS = 0.0f;
    float nu = 0.0f;
    bool rayIntersectsGround = false;
    AtmosphereGetRMuMuSNuFromScatteringFragCoord(
        Params, float3(dispatchThreadId) + 0.5f.xxx, r, mu, muS, nu, rayIntersectsGround);

    float3 rayleigh = 0.0f.xxx;
    float3 mie = 0.0f.xxx;
    AtmosphereComputeSingleScattering(Params, TransmittanceLut, LinearSampler,
        r, mu, muS, nu, rayIntersectsGround, rayleigh, mie);

    OutSingleRayleighScattering[dispatchThreadId] = float4(rayleigh, 0.0f);
    OutScattering[dispatchThreadId] = float4(rayleigh, 0.0f);
    OutSingleMieScattering[dispatchThreadId] = float4(mie, 0.0f);
}
