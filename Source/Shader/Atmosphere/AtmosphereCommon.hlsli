#pragma once

#include "Shader/Bindings/ShaderBindings.hlsli"

struct FAtmosphereParams
{
    float4 RayleighScattering_RayleighScaleHeightKm;
    float4 MieScattering_MieScaleHeightKm;
    float4 MieAbsorption_MieAnisotropy;
    float4 OzoneAbsorption_OzoneCenterHeightKm;
    float4 GroundAlbedo_OzoneThicknessKm;
    float4 SolarTint_SolarIlluminance;
    float4 SunDirection_Roughness;
    float4 Geometry;
    float4 Output;
};

static const uint AE_ATMOSPHERE_OUTPUT_SKY        = 0u;
static const uint AE_ATMOSPHERE_OUTPUT_IRRADIANCE = 1u;
static const uint AE_ATMOSPHERE_OUTPUT_SPECULAR   = 2u;

float3 AtmosphereSafeNormalize(float3 v)
{
    const float lenSq = max(dot(v, v), 1e-6f);
    return v * rsqrt(lenSq);
}

float3 AtmosphereRayleighScattering(FAtmosphereParams params)
{
    return params.RayleighScattering_RayleighScaleHeightKm.xyz;
}

float AtmosphereRayleighScaleHeightKm(FAtmosphereParams params)
{
    return params.RayleighScattering_RayleighScaleHeightKm.w;
}

float3 AtmosphereMieScattering(FAtmosphereParams params)
{
    return params.MieScattering_MieScaleHeightKm.xyz;
}

float AtmosphereMieScaleHeightKm(FAtmosphereParams params)
{
    return params.MieScattering_MieScaleHeightKm.w;
}

float3 AtmosphereMieAbsorption(FAtmosphereParams params)
{
    return params.MieAbsorption_MieAnisotropy.xyz;
}

float AtmosphereMieAnisotropy(FAtmosphereParams params)
{
    return params.MieAbsorption_MieAnisotropy.w;
}

float3 AtmosphereOzoneAbsorption(FAtmosphereParams params)
{
    return params.OzoneAbsorption_OzoneCenterHeightKm.xyz;
}

float AtmosphereOzoneCenterHeightKm(FAtmosphereParams params)
{
    return params.OzoneAbsorption_OzoneCenterHeightKm.w;
}

float3 AtmosphereGroundAlbedo(FAtmosphereParams params)
{
    return params.GroundAlbedo_OzoneThicknessKm.xyz;
}

float AtmosphereOzoneThicknessKm(FAtmosphereParams params)
{
    return params.GroundAlbedo_OzoneThicknessKm.w;
}

float3 AtmosphereSolarTint(FAtmosphereParams params)
{
    return params.SolarTint_SolarIlluminance.xyz;
}

float AtmosphereSolarIlluminance(FAtmosphereParams params)
{
    return params.SolarTint_SolarIlluminance.w;
}

float3 AtmosphereSunDirection(FAtmosphereParams params)
{
    return AtmosphereSafeNormalize(params.SunDirection_Roughness.xyz);
}

float AtmosphereRoughness(FAtmosphereParams params)
{
    return saturate(params.SunDirection_Roughness.w);
}

float AtmosphereSunAngularRadius(FAtmosphereParams params)
{
    return params.Geometry.x;
}

float AtmospherePlanetRadiusKm(FAtmosphereParams params)
{
    return params.Geometry.y;
}

float AtmosphereHeightKm(FAtmosphereParams params)
{
    return params.Geometry.z;
}

float AtmosphereViewHeightKm(FAtmosphereParams params)
{
    return params.Geometry.w;
}

float AtmosphereExposure(FAtmosphereParams params)
{
    return params.Output.x;
}

uint AtmosphereOutputMode(FAtmosphereParams params)
{
    return (uint)(params.Output.y + 0.5f);
}

float3 AtmosphereFaceUvToDirection(uint face, float2 uv)
{
    const float u = uv.x;
    const float v = uv.y;
    if (face == 0u) return AtmosphereSafeNormalize(float3(1.0f, v, -u));
    if (face == 1u) return AtmosphereSafeNormalize(float3(-1.0f, v, u));
    if (face == 2u) return AtmosphereSafeNormalize(float3(u, 1.0f, -v));
    if (face == 3u) return AtmosphereSafeNormalize(float3(u, -1.0f, v));
    if (face == 4u) return AtmosphereSafeNormalize(float3(u, v, 1.0f));
    return AtmosphereSafeNormalize(float3(-u, v, -1.0f));
}

float3 ComputeAtmosphereColor(FAtmosphereParams params, float3 direction, float roughness)
{
    const float kPi            = 3.1415926535f;
    const float kRayleighScale = 3.0f / (16.0f * kPi);
    const float kMieScale      = 1.0f / (4.0f * kPi);

    const float3 dir    = AtmosphereSafeNormalize(direction);
    const float3 sunDir = AtmosphereSunDirection(params);
    const float  mu     = clamp(dot(dir, sunDir), -1.0f, 1.0f);
    const float  mu2    = mu * mu;
    const float  g      = clamp(AtmosphereMieAnisotropy(params), 0.0f, 0.98f);
    const float  g2     = g * g;
    const float  denom  = max(1.0f + g2 - 2.0f * g * mu, 1e-3f);

    const float  rayleighPhase = kRayleighScale * (1.0f + mu2);
    const float  miePhase = kMieScale * ((1.0f - g2) * (1.0f + mu2))
        / (max(pow(denom, 1.5f), 1e-3f) * (2.0f + g2));

    const float  atmosphereHeight = max(AtmosphereHeightKm(params), 1e-3f);
    const float  viewHeightKm = clamp(AtmosphereViewHeightKm(params), 0.0f, atmosphereHeight);
    const float  altitudeNorm  = saturate(viewHeightKm / atmosphereHeight);
    const float  horizonFactor = saturate(1.0f - dir.y * 0.5f - 0.5f);
    const float  zenithFactor  = saturate(dir.y * 0.5f + 0.5f);

    const float  rayleighDensity =
        exp(-viewHeightKm / max(AtmosphereRayleighScaleHeightKm(params), 1e-3f));
    const float  mieDensity =
        exp(-viewHeightKm / max(AtmosphereMieScaleHeightKm(params), 1e-3f));
    const float  ozoneDensity = saturate(1.0f
        - abs(viewHeightKm - AtmosphereOzoneCenterHeightKm(params))
            / max(AtmosphereOzoneThicknessKm(params), 1e-3f));

    const float  pathLength = 0.2f + 1.8f * horizonFactor;

    float3 transmittance = 1.0f.xxx;
    [unroll]
    for (uint i = 0u; i < 3u; ++i)
    {
        const float extinction = AtmosphereRayleighScattering(params)[i] * rayleighDensity
            + (AtmosphereMieScattering(params)[i] + AtmosphereMieAbsorption(params)[i]) * mieDensity
            + AtmosphereOzoneAbsorption(params)[i] * ozoneDensity;
        transmittance[i] = exp(-extinction * pathLength * 32.0f);
    }

    float3 scattering = AtmosphereRayleighScattering(params) * (rayleighPhase * rayleighDensity)
        + AtmosphereMieScattering(params) * (miePhase * mieDensity * 0.5f);
    scattering *= AtmosphereSolarIlluminance(params);
    scattering *= AtmosphereSolarTint(params);

    const float  sunDisk = exp(
        -(1.0f - mu) / max(AtmosphereSunAngularRadius(params) * (0.35f + roughness), 1e-4f));
    const float3 directSun =
        AtmosphereSolarTint(params) * (AtmosphereSolarIlluminance(params) * sunDisk * 0.18f);

    const float3 zenithTint =
        (AtmosphereRayleighScattering(params) * 12.0f + AtmosphereSolarTint(params) * 0.4f + 0.02f.xxx) / 3.0f;
    const float3 horizonTint =
        ((AtmosphereMieScattering(params) * 20.0f + AtmosphereGroundAlbedo(params) * 0.5f)
            + AtmosphereSolarTint(params) * 0.8f + float3(0.08f, 0.10f, 0.14f)) / 3.0f;
    float3 ambient = lerp(horizonTint, zenithTint, zenithFactor);
    ambient *= 0.5f + 0.5f * (1.0f - altitudeNorm);

    float3 color = (ambient + scattering) * transmittance + directSun
        + AtmosphereGroundAlbedo(params) * (saturate(-dir.y) * 0.25f);
    color *= AtmosphereExposure(params);

    if (roughness > 0.0f)
    {
        const float3 overcast = AtmosphereSolarTint(params) * (0.08f * AtmosphereSolarIlluminance(params))
            + AtmosphereGroundAlbedo(params) * 0.15f;
        color = lerp(color, overcast, saturate(roughness));
    }

    return max(color, 0.0f.xxx);
}

float3 SampleAtmosphereSky(TextureCube skyTexture, SamplerState skySampler, float3 direction)
{
    return skyTexture.SampleLevel(skySampler, AtmosphereSafeNormalize(direction), 0.0f).rgb;
}
