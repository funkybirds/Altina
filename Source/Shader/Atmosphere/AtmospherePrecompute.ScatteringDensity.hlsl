#include "Shader/Atmosphere/AtmosphereBruneton.hlsli"

AE_PER_FRAME_CBUFFER(AtmosphereParams)
{
    FAtmosphereParams Params;
};

AE_PER_FRAME_SRV(Texture2D<float4>, TransmittanceLut);
AE_PER_FRAME_SRV(Texture3D<float4>, SingleRayleighScatteringLut);
AE_PER_FRAME_SRV(Texture3D<float4>, SingleMieScatteringLut);
AE_PER_FRAME_SRV(Texture3D<float4>, MultipleScatteringLut);
AE_PER_FRAME_SRV(Texture2D<float4>, IrradianceLut);
AE_PER_FRAME_SAMPLER(LinearSampler);
AE_PER_FRAME_UAV(RWTexture3D<float4>, OutScatteringDensity);

[numthreads(16, 16, 1)]
void CSAtmosphereScatteringDensity(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint width;
    uint height;
    uint depth;
    OutScatteringDensity.GetDimensions(width, height, depth);
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

    const float3 scatteringDensity = AtmosphereComputeScatteringDensity(Params, TransmittanceLut,
        SingleRayleighScatteringLut, SingleMieScatteringLut, MultipleScatteringLut,
        IrradianceLut, LinearSampler, r, mu, muS, nu, AtmosphereScatteringOrder(Params));
    OutScatteringDensity[dispatchThreadId] = float4(scatteringDensity, 1.0f);
}
