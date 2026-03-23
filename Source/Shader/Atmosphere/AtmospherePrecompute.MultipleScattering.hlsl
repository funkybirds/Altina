#include "Shader/Atmosphere/AtmosphereBruneton.hlsli"

AE_PER_FRAME_CBUFFER(AtmosphereParams)
{
    FAtmosphereParams Params;
};

AE_PER_FRAME_SRV(Texture2D<float4>, TransmittanceLut);
AE_PER_FRAME_SRV(Texture3D<float4>, ScatteringDensityLut);
AE_PER_FRAME_SAMPLER(LinearSampler);
AE_PER_FRAME_UAV(RWTexture3D<float4>, OutDeltaMultipleScattering);
AE_PER_FRAME_UAV(RWTexture3D<float4>, OutScattering);

[numthreads(16, 16, 1)]
void CSAtmosphereMultipleScattering(uint3 dispatchThreadId : SV_DispatchThreadID)
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

    const float3 deltaMultipleScattering = AtmosphereComputeMultipleScattering(Params,
        TransmittanceLut, ScatteringDensityLut, LinearSampler, r, mu, muS, nu, rayIntersectsGround);
    const float rayleighPhase = max(AtmosphereRayleighPhaseFunction(nu), 1e-4f);
    const float3 accumulatedScattering = OutScattering[dispatchThreadId].rgb
        + deltaMultipleScattering / rayleighPhase;

    OutDeltaMultipleScattering[dispatchThreadId] = float4(deltaMultipleScattering, 0.0f);
    OutScattering[dispatchThreadId] = float4(accumulatedScattering, 0.0f);
}
