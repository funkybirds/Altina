#pragma once

#include "Shader/Atmosphere/AtmosphereCommon.hlsli"

static const uint AE_ATMOSPHERE_TRANSMITTANCE_WIDTH   = 256u;
static const uint AE_ATMOSPHERE_TRANSMITTANCE_HEIGHT  = 64u;
static const uint AE_ATMOSPHERE_IRRADIANCE_WIDTH      = 64u;
static const uint AE_ATMOSPHERE_IRRADIANCE_HEIGHT     = 16u;
static const uint AE_ATMOSPHERE_SCATTERING_R_SIZE     = 32u;
static const uint AE_ATMOSPHERE_SCATTERING_MU_SIZE    = 128u;
static const uint AE_ATMOSPHERE_SCATTERING_MU_S_SIZE  = 32u;
static const uint AE_ATMOSPHERE_SCATTERING_NU_SIZE    = 8u;
static const uint AE_ATMOSPHERE_SCATTERING_WIDTH =
    AE_ATMOSPHERE_SCATTERING_R_SIZE * AE_ATMOSPHERE_SCATTERING_MU_S_SIZE;
static const uint AE_ATMOSPHERE_SCATTERING_HEIGHT = AE_ATMOSPHERE_SCATTERING_MU_SIZE;
static const uint AE_ATMOSPHERE_SCATTERING_DEPTH  = AE_ATMOSPHERE_SCATTERING_R_SIZE;

static const float AE_ATMOSPHERE_PI = 3.14159265358979323846f;
static const float AE_ATMOSPHERE_MU_S_MIN = -0.2f;

uint AtmosphereScatteringOrder(FAtmosphereParams params)
{
    return (uint)(params.Output.y + 0.5f);
}

float3 AtmosphereSolarIrradiance(FAtmosphereParams params)
{
    return max(AtmosphereSolarTint(params) * AtmosphereSolarIlluminance(params), 0.0f.xxx);
}

float AtmosphereBottomRadius(FAtmosphereParams params)
{
    return AtmospherePlanetRadiusKm(params);
}

float AtmosphereTopRadius(FAtmosphereParams params)
{
    return AtmospherePlanetRadiusKm(params) + AtmosphereHeightKm(params);
}

float AtmosphereViewRadius(FAtmosphereParams params)
{
    return AtmospherePlanetRadiusKm(params) + AtmosphereViewHeightKm(params);
}

float AtmosphereSunAngularRadiusRad(FAtmosphereParams params)
{
    return AtmosphereSunAngularRadius(params);
}

float AtmosphereClampCosine(float mu)
{
    return clamp(mu, -1.0f, 1.0f);
}

float AtmosphereClampDistance(float d)
{
    return max(d, 0.0f);
}

float AtmosphereSafeSqrt(float v)
{
    return sqrt(max(v, 0.0f));
}

float AtmosphereClampRadius(FAtmosphereParams params, float r)
{
    return clamp(r, AtmosphereBottomRadius(params), AtmosphereTopRadius(params));
}

float AtmosphereGetTextureCoordFromUnitRange(float x, uint textureSize)
{
    return 0.5f / float(textureSize) + x * (1.0f - 1.0f / float(textureSize));
}

float AtmosphereGetUnitRangeFromTextureCoord(float u, uint textureSize)
{
    return (u - 0.5f / float(textureSize)) / (1.0f - 1.0f / float(textureSize));
}

float AtmosphereGetRayleighDensity(FAtmosphereParams params, float altitude)
{
    const float scaleHeight = max(AtmosphereRayleighScaleHeightKm(params), 1e-3f);
    return exp(-max(altitude, 0.0f) / scaleHeight);
}

float AtmosphereGetMieDensity(FAtmosphereParams params, float altitude)
{
    const float scaleHeight = max(AtmosphereMieScaleHeightKm(params), 1e-3f);
    return exp(-max(altitude, 0.0f) / scaleHeight);
}

float AtmosphereGetAbsorptionDensity(FAtmosphereParams params, float altitude)
{
    const float center = AtmosphereOzoneCenterHeightKm(params);
    const float width = max(AtmosphereOzoneThicknessKm(params), 1e-3f);
    return saturate(1.0f - abs(altitude - center) / width);
}

float AtmosphereDistanceToTopBoundary(FAtmosphereParams params, float r, float mu)
{
    const float topRadius = AtmosphereTopRadius(params);
    const float discriminant = r * r * (mu * mu - 1.0f) + topRadius * topRadius;
    return AtmosphereClampDistance(-r * mu + AtmosphereSafeSqrt(discriminant));
}

float AtmosphereDistanceToBottomBoundary(FAtmosphereParams params, float r, float mu)
{
    const float bottomRadius = AtmosphereBottomRadius(params);
    const float discriminant = r * r * (mu * mu - 1.0f) + bottomRadius * bottomRadius;
    return AtmosphereClampDistance(-r * mu - AtmosphereSafeSqrt(discriminant));
}

bool AtmosphereRayIntersectsGround(FAtmosphereParams params, float r, float mu)
{
    const float bottomRadius = AtmosphereBottomRadius(params);
    return mu < 0.0f && r * r * (mu * mu - 1.0f) + bottomRadius * bottomRadius >= 0.0f;
}

float AtmosphereDistanceToNearestBoundary(
    FAtmosphereParams params, float r, float mu, bool rayIntersectsGround)
{
    return rayIntersectsGround ? AtmosphereDistanceToBottomBoundary(params, r, mu)
                               : AtmosphereDistanceToTopBoundary(params, r, mu);
}

float AtmosphereComputeOpticalLengthToTopBoundary(
    FAtmosphereParams params, float r, float mu, uint profileMode)
{
    const int sampleCount = 128;
    const float dx = AtmosphereDistanceToTopBoundary(params, r, mu) / float(sampleCount);
    float result = 0.0f;
    [loop]
    for (int i = 0; i <= sampleCount; ++i)
    {
        const float d = float(i) * dx;
        const float rI = sqrt(d * d + 2.0f * r * mu * d + r * r);
        const float altitude = max(rI - AtmosphereBottomRadius(params), 0.0f);
        float density = 0.0f;
        if (profileMode == 0u)
        {
            density = AtmosphereGetRayleighDensity(params, altitude);
        }
        else if (profileMode == 1u)
        {
            density = AtmosphereGetMieDensity(params, altitude);
        }
        else
        {
            density = AtmosphereGetAbsorptionDensity(params, altitude);
        }
        const float weight = (i == 0 || i == sampleCount) ? 0.5f : 1.0f;
        result += density * weight * dx;
    }
    return result;
}

float3 AtmosphereComputeTransmittanceToTopBoundary(FAtmosphereParams params, float r, float mu)
{
    const float3 opticalDepth =
        AtmosphereRayleighScattering(params)
            * AtmosphereComputeOpticalLengthToTopBoundary(params, r, mu, 0u)
        + (AtmosphereMieScattering(params) + AtmosphereMieAbsorption(params))
            * AtmosphereComputeOpticalLengthToTopBoundary(params, r, mu, 1u)
        + AtmosphereOzoneAbsorption(params)
            * AtmosphereComputeOpticalLengthToTopBoundary(params, r, mu, 2u);
    return exp(-opticalDepth);
}

float2 AtmosphereGetTransmittanceUvFromRMu(FAtmosphereParams params, float r, float mu)
{
    const float bottomRadius = AtmosphereBottomRadius(params);
    const float topRadius = AtmosphereTopRadius(params);
    const float H = sqrt(topRadius * topRadius - bottomRadius * bottomRadius);
    const float rho = AtmosphereSafeSqrt(r * r - bottomRadius * bottomRadius);
    const float d = AtmosphereDistanceToTopBoundary(params, r, mu);
    const float dMin = topRadius - r;
    const float dMax = rho + H;
    const float xMu = (dMax == dMin) ? 0.0f : (d - dMin) / (dMax - dMin);
    const float xR = rho / max(H, 1.0f);
    return float2(
        AtmosphereGetTextureCoordFromUnitRange(xMu, AE_ATMOSPHERE_TRANSMITTANCE_WIDTH),
        AtmosphereGetTextureCoordFromUnitRange(xR, AE_ATMOSPHERE_TRANSMITTANCE_HEIGHT));
}

void AtmosphereGetRMuFromTransmittanceUv(
    FAtmosphereParams params, float2 uv, out float r, out float mu)
{
    const float xMu = AtmosphereGetUnitRangeFromTextureCoord(
        uv.x, AE_ATMOSPHERE_TRANSMITTANCE_WIDTH);
    const float xR = AtmosphereGetUnitRangeFromTextureCoord(
        uv.y, AE_ATMOSPHERE_TRANSMITTANCE_HEIGHT);
    const float bottomRadius = AtmosphereBottomRadius(params);
    const float topRadius = AtmosphereTopRadius(params);
    const float H = sqrt(topRadius * topRadius - bottomRadius * bottomRadius);
    const float rho = H * xR;
    r = sqrt(rho * rho + bottomRadius * bottomRadius);
    const float dMin = topRadius - r;
    const float dMax = rho + H;
    const float d = dMin + xMu * (dMax - dMin);
    mu = (d == 0.0f) ? 1.0f : (H * H - rho * rho - d * d) / (2.0f * r * d);
    mu = AtmosphereClampCosine(mu);
}

float3 AtmosphereSampleTransmittanceToTopBoundary(
    FAtmosphereParams params, Texture2D<float4> transmittanceTexture, SamplerState linearSampler,
    float r, float mu)
{
    return transmittanceTexture.SampleLevel(
        linearSampler, AtmosphereGetTransmittanceUvFromRMu(params, r, mu), 0.0f).rgb;
}

float3 AtmosphereGetTransmittance(
    FAtmosphereParams params, Texture2D<float4> transmittanceTexture, SamplerState linearSampler,
    float r, float mu, float d, bool rayIntersectsGround)
{
    const float rD = AtmosphereClampRadius(params, sqrt(d * d + 2.0f * r * mu * d + r * r));
    const float muD = AtmosphereClampCosine((r * mu + d) / max(rD, 1.0f));
    if (rayIntersectsGround)
    {
        return min(
            AtmosphereSampleTransmittanceToTopBoundary(
                params, transmittanceTexture, linearSampler, rD, -muD)
                / max(AtmosphereSampleTransmittanceToTopBoundary(
                          params, transmittanceTexture, linearSampler, r, -mu),
                    1e-6f.xxx),
            1.0f.xxx);
    }

    return min(
        AtmosphereSampleTransmittanceToTopBoundary(
            params, transmittanceTexture, linearSampler, r, mu)
            / max(AtmosphereSampleTransmittanceToTopBoundary(
                      params, transmittanceTexture, linearSampler, rD, muD),
                1e-6f.xxx),
        1.0f.xxx);
}

float3 AtmosphereGetTransmittanceToSun(
    FAtmosphereParams params, Texture2D<float4> transmittanceTexture, SamplerState linearSampler,
    float r, float muS)
{
    const float sinThetaH = AtmosphereBottomRadius(params) / r;
    const float cosThetaH = -sqrt(max(1.0f - sinThetaH * sinThetaH, 0.0f));
    return AtmosphereSampleTransmittanceToTopBoundary(
               params, transmittanceTexture, linearSampler, r, muS)
        * smoothstep(-sinThetaH * AtmosphereSunAngularRadiusRad(params),
            sinThetaH * AtmosphereSunAngularRadiusRad(params), muS - cosThetaH);
}

float AtmosphereRayleighPhaseFunction(float nu)
{
    return 3.0f / (16.0f * AE_ATMOSPHERE_PI) * (1.0f + nu * nu);
}

float AtmosphereMiePhaseFunction(float g, float nu)
{
    const float g2 = g * g;
    return 3.0f / (8.0f * AE_ATMOSPHERE_PI) * (1.0f - g2) / (2.0f + g2)
        * (1.0f + nu * nu) / pow(max(1.0f + g2 - 2.0f * g * nu, 1e-3f), 1.5f);
}

void AtmosphereGetScatteringUvwz(
    FAtmosphereParams params, float r, float mu, float muS, float nu,
    bool rayIntersectsGround, out float4 uvwz)
{
    const float bottomRadius = AtmosphereBottomRadius(params);
    const float topRadius = AtmosphereTopRadius(params);
    const float H = sqrt(topRadius * topRadius - bottomRadius * bottomRadius);
    const float rho = AtmosphereSafeSqrt(r * r - bottomRadius * bottomRadius);
    const float uR = AtmosphereGetTextureCoordFromUnitRange(
        rho / max(H, 1.0f), AE_ATMOSPHERE_SCATTERING_R_SIZE);

    const float rMu = r * mu;
    const float discriminant = rMu * rMu - r * r + bottomRadius * bottomRadius;
    float uMu = 0.0f;
    if (rayIntersectsGround)
    {
        const float d = -rMu - AtmosphereSafeSqrt(discriminant);
        const float dMin = r - bottomRadius;
        const float dMax = rho;
        const float x = (dMax == dMin) ? 0.0f : (d - dMin) / (dMax - dMin);
        uMu = 0.5f - 0.5f * AtmosphereGetTextureCoordFromUnitRange(
            x, AE_ATMOSPHERE_SCATTERING_MU_SIZE / 2u);
    }
    else
    {
        const float d = -rMu + AtmosphereSafeSqrt(discriminant + H * H);
        const float dMin = topRadius - r;
        const float dMax = rho + H;
        uMu = 0.5f + 0.5f * AtmosphereGetTextureCoordFromUnitRange(
            (d - dMin) / max(dMax - dMin, 1.0f), AE_ATMOSPHERE_SCATTERING_MU_SIZE / 2u);
    }

    const float d = AtmosphereDistanceToTopBoundary(params, bottomRadius, muS);
    const float dMin = topRadius - bottomRadius;
    const float dMax = H;
    const float a = (d - dMin) / max(dMax - dMin, 1.0f);
    const float D = AtmosphereDistanceToTopBoundary(params, bottomRadius, AE_ATMOSPHERE_MU_S_MIN);
    const float A = (D - dMin) / max(dMax - dMin, 1.0f);
    const float uMuS = AtmosphereGetTextureCoordFromUnitRange(
        max(1.0f - a / max(A, 1e-3f), 0.0f) / (1.0f + a), AE_ATMOSPHERE_SCATTERING_MU_S_SIZE);

    uvwz = float4((nu + 1.0f) * 0.5f, uMuS, uMu, uR);
}

void AtmosphereGetRMuMuSNuFromScatteringFragCoord(
    FAtmosphereParams params, float3 fragCoord, out float r, out float mu, out float muS,
    out float nu, out bool rayIntersectsGround)
{
    const float4 scatteringSize = float4(
        float(AE_ATMOSPHERE_SCATTERING_NU_SIZE - 1u),
        float(AE_ATMOSPHERE_SCATTERING_MU_S_SIZE),
        float(AE_ATMOSPHERE_SCATTERING_MU_SIZE),
        float(AE_ATMOSPHERE_SCATTERING_R_SIZE));

    const float fragCoordNu = floor(fragCoord.x / float(AE_ATMOSPHERE_SCATTERING_MU_S_SIZE));
    const float fragCoordMuS = fmod(fragCoord.x, float(AE_ATMOSPHERE_SCATTERING_MU_S_SIZE));
    const float4 uvwz = float4(fragCoordNu, fragCoordMuS, fragCoord.y, fragCoord.z) / scatteringSize;

    const float bottomRadius = AtmosphereBottomRadius(params);
    const float topRadius = AtmosphereTopRadius(params);
    const float H = sqrt(topRadius * topRadius - bottomRadius * bottomRadius);
    const float rho = H * AtmosphereGetUnitRangeFromTextureCoord(
        uvwz.w, AE_ATMOSPHERE_SCATTERING_R_SIZE);
    r = sqrt(rho * rho + bottomRadius * bottomRadius);

    if (uvwz.z < 0.5f)
    {
        const float dMin = r - bottomRadius;
        const float dMax = rho;
        const float xMu = AtmosphereGetUnitRangeFromTextureCoord(
            1.0f - 2.0f * uvwz.z, AE_ATMOSPHERE_SCATTERING_MU_SIZE / 2u);
        const float d = dMin + xMu * (dMax - dMin);
        mu = (d == 0.0f) ? -1.0f : -(rho * rho + d * d) / (2.0f * r * d);
        rayIntersectsGround = true;
    }
    else
    {
        const float dMin = topRadius - r;
        const float dMax = rho + H;
        const float xMu = AtmosphereGetUnitRangeFromTextureCoord(
            2.0f * uvwz.z - 1.0f, AE_ATMOSPHERE_SCATTERING_MU_SIZE / 2u);
        const float d = dMin + xMu * (dMax - dMin);
        mu = (d == 0.0f) ? 1.0f : (H * H - rho * rho - d * d) / (2.0f * r * d);
        rayIntersectsGround = false;
    }
    mu = AtmosphereClampCosine(mu);

    const float dMin = topRadius - bottomRadius;
    const float dMax = H;
    const float xMuS = AtmosphereGetUnitRangeFromTextureCoord(
        uvwz.y, AE_ATMOSPHERE_SCATTERING_MU_S_SIZE);
    const float D = AtmosphereDistanceToTopBoundary(params, bottomRadius, AE_ATMOSPHERE_MU_S_MIN);
    const float A = (D - dMin) / max(dMax - dMin, 1.0f);
    const float a = (A - xMuS * A) / (1.0f + xMuS * A);
    const float d = dMin + min(a, A) * (dMax - dMin);
    muS = (d == 0.0f) ? 1.0f
        : AtmosphereClampCosine((H * H - d * d) / (2.0f * bottomRadius * d));

    nu = AtmosphereClampCosine(uvwz.x * 2.0f - 1.0f);
    const float nuMin = mu * muS - sqrt(max((1.0f - mu * mu) * (1.0f - muS * muS), 0.0f));
    const float nuMax = mu * muS + sqrt(max((1.0f - mu * mu) * (1.0f - muS * muS), 0.0f));
    nu = clamp(nu, nuMin, nuMax);
}

void AtmosphereComputeSingleScattering(
    FAtmosphereParams params, Texture2D<float4> transmittanceTexture, SamplerState linearSampler,
    float r, float mu, float muS, float nu, bool rayIntersectsGround,
    out float3 rayleigh, out float3 mie)
{
    const int sampleCount = 50;
    const float dx = AtmosphereDistanceToNearestBoundary(params, r, mu, rayIntersectsGround)
        / float(sampleCount);
    float3 rayleighSum = 0.0f.xxx;
    float3 mieSum = 0.0f.xxx;

    [loop]
    for (int i = 0; i <= sampleCount; ++i)
    {
        const float d = float(i) * dx;
        const float rD = AtmosphereClampRadius(params, sqrt(d * d + 2.0f * r * mu * d + r * r));
        const float muSD = AtmosphereClampCosine((r * muS + d * nu) / max(rD, 1.0f));
        const float altitude = max(rD - AtmosphereBottomRadius(params), 0.0f);
        const float3 transmittance = AtmosphereGetTransmittance(
                params, transmittanceTexture, linearSampler, r, mu, d, rayIntersectsGround)
            * AtmosphereGetTransmittanceToSun(
                params, transmittanceTexture, linearSampler, rD, muSD);
        const float weight = (i == 0 || i == sampleCount) ? 0.5f : 1.0f;
        rayleighSum += transmittance * AtmosphereGetRayleighDensity(params, altitude) * weight;
        mieSum += transmittance * AtmosphereGetMieDensity(params, altitude) * weight;
    }

    rayleigh = rayleighSum * dx * AtmosphereSolarIrradiance(params) * AtmosphereRayleighScattering(params);
    mie = mieSum * dx * AtmosphereSolarIrradiance(params) * AtmosphereMieScattering(params);
}

float3 AtmosphereSamplePackedScattering(
    Texture3D<float4> scatteringTexture, SamplerState linearSampler, float4 uvwz)
{
    const float texCoordX = uvwz.x * float(AE_ATMOSPHERE_SCATTERING_NU_SIZE - 1u);
    const float texX = floor(texCoordX);
    const float lerpX = texCoordX - texX;
    const float3 uvw0 = float3(
        (texX + uvwz.y) / float(AE_ATMOSPHERE_SCATTERING_NU_SIZE), uvwz.z, uvwz.w);
    const float3 uvw1 = float3(
        (texX + 1.0f + uvwz.y) / float(AE_ATMOSPHERE_SCATTERING_NU_SIZE), uvwz.z, uvwz.w);
    return lerp(scatteringTexture.SampleLevel(linearSampler, uvw0, 0.0f).rgb,
        scatteringTexture.SampleLevel(linearSampler, uvw1, 0.0f).rgb, lerpX);
}

float3 AtmosphereGetScattering(
    FAtmosphereParams params, Texture3D<float4> scatteringTexture, SamplerState linearSampler,
    float r, float mu, float muS, float nu, bool rayIntersectsGround)
{
    float4 uvwz = 0.0f.xxxx;
    AtmosphereGetScatteringUvwz(params, r, mu, muS, nu, rayIntersectsGround, uvwz);
    return AtmosphereSamplePackedScattering(scatteringTexture, linearSampler, uvwz);
}

float3 AtmosphereGetIrradiance(
    FAtmosphereParams params, Texture2D<float4> irradianceTexture, SamplerState linearSampler,
    float r, float muS)
{
    return irradianceTexture.SampleLevel(
        linearSampler, AtmosphereGetIrradianceUvFromRMuS(params, r, muS), 0.0f).rgb;
}

float3 AtmosphereGetCombinedScattering(
    FAtmosphereParams params, Texture3D<float4> scatteringTexture,
    Texture3D<float4> singleMieScatteringTexture, SamplerState linearSampler,
    float r, float mu, float muS, float nu, bool rayIntersectsGround,
    out float3 singleMieScattering)
{
    float4 uvwz = 0.0f.xxxx;
    AtmosphereGetScatteringUvwz(params, r, mu, muS, nu, rayIntersectsGround, uvwz);
    const float texCoordX = uvwz.x * float(AE_ATMOSPHERE_SCATTERING_NU_SIZE - 1u);
    const float texX = floor(texCoordX);
    const float lerpX = texCoordX - texX;
    const float3 uvw0 = float3(
        (texX + uvwz.y) / float(AE_ATMOSPHERE_SCATTERING_NU_SIZE), uvwz.z, uvwz.w);
    const float3 uvw1 = float3(
        (texX + 1.0f + uvwz.y) / float(AE_ATMOSPHERE_SCATTERING_NU_SIZE), uvwz.z, uvwz.w);

    const float3 scattering = lerp(
        scatteringTexture.SampleLevel(linearSampler, uvw0, 0.0f).rgb,
        scatteringTexture.SampleLevel(linearSampler, uvw1, 0.0f).rgb, lerpX);
    singleMieScattering = lerp(
        singleMieScatteringTexture.SampleLevel(linearSampler, uvw0, 0.0f).rgb,
        singleMieScatteringTexture.SampleLevel(linearSampler, uvw1, 0.0f).rgb, lerpX);
    return scattering;
}

float3 AtmosphereGetScatteringByOrder(
    FAtmosphereParams params, Texture3D<float4> singleRayleighScatteringTexture,
    Texture3D<float4> singleMieScatteringTexture, Texture3D<float4> multipleScatteringTexture,
    SamplerState linearSampler, float r, float mu, float muS, float nu, bool rayIntersectsGround,
    uint scatteringOrder)
{
    if (scatteringOrder <= 1u)
    {
        const float3 rayleigh = AtmosphereGetScattering(
            params, singleRayleighScatteringTexture, linearSampler, r, mu, muS, nu, rayIntersectsGround);
        const float3 mie = AtmosphereGetScattering(
            params, singleMieScatteringTexture, linearSampler, r, mu, muS, nu, rayIntersectsGround);
        return rayleigh * AtmosphereRayleighPhaseFunction(nu)
            + mie * AtmosphereMiePhaseFunction(saturate(AtmosphereMieAnisotropy(params)), nu);
    }

    return AtmosphereGetScattering(
        params, multipleScatteringTexture, linearSampler, r, mu, muS, nu, rayIntersectsGround);
}

float3 AtmosphereGetSkyRadiance(
    FAtmosphereParams params, Texture2D<float4> transmittanceTexture,
    Texture3D<float4> scatteringTexture, Texture3D<float4> singleMieScatteringTexture,
    SamplerState linearSampler, float3 camera, float3 viewRay, float3 sunDirection,
    out float3 transmittance)
{
    float r = length(camera);
    float rMu = dot(camera, viewRay);
    const float topRadius = AtmosphereTopRadius(params);
    const float distanceToTopBoundary =
        -rMu - sqrt(max(rMu * rMu - r * r + topRadius * topRadius, 0.0f));
    if (distanceToTopBoundary > 0.0f)
    {
        camera += viewRay * distanceToTopBoundary;
        r = topRadius;
        rMu += distanceToTopBoundary;
    }
    else if (r > topRadius)
    {
        transmittance = 1.0f.xxx;
        return 0.0f.xxx;
    }

    const float mu = rMu / max(r, 1.0f);
    const float muS = dot(camera, sunDirection) / max(r, 1.0f);
    const float nu = dot(viewRay, sunDirection);
    const bool rayIntersectsGround = AtmosphereRayIntersectsGround(params, r, mu);

    transmittance = rayIntersectsGround ? 0.0f.xxx
        : AtmosphereSampleTransmittanceToTopBoundary(
            params, transmittanceTexture, linearSampler, r, mu);

    float3 singleMieScattering = 0.0f.xxx;
    const float3 scattering = AtmosphereGetCombinedScattering(params, scatteringTexture,
        singleMieScatteringTexture, linearSampler, r, mu, muS, nu, rayIntersectsGround,
        singleMieScattering);

    return scattering * AtmosphereRayleighPhaseFunction(nu)
        + singleMieScattering * AtmosphereMiePhaseFunction(
            saturate(AtmosphereMieAnisotropy(params)), nu);
}

float2 AtmosphereGetIrradianceUvFromRMuS(FAtmosphereParams params, float r, float muS)
{
    const float xR =
        (r - AtmosphereBottomRadius(params)) / max(AtmosphereTopRadius(params) - AtmosphereBottomRadius(params), 1.0f);
    const float xMuS = muS * 0.5f + 0.5f;
    return float2(
        AtmosphereGetTextureCoordFromUnitRange(xMuS, AE_ATMOSPHERE_IRRADIANCE_WIDTH),
        AtmosphereGetTextureCoordFromUnitRange(xR, AE_ATMOSPHERE_IRRADIANCE_HEIGHT));
}

void AtmosphereGetRMuSFromIrradianceUv(
    FAtmosphereParams params, float2 uv, out float r, out float muS)
{
    const float xMuS = AtmosphereGetUnitRangeFromTextureCoord(
        uv.x, AE_ATMOSPHERE_IRRADIANCE_WIDTH);
    const float xR = AtmosphereGetUnitRangeFromTextureCoord(
        uv.y, AE_ATMOSPHERE_IRRADIANCE_HEIGHT);
    r = AtmosphereBottomRadius(params)
        + xR * (AtmosphereTopRadius(params) - AtmosphereBottomRadius(params));
    muS = AtmosphereClampCosine(2.0f * xMuS - 1.0f);
}

float3 AtmosphereComputeDirectIrradiance(
    FAtmosphereParams params, Texture2D<float4> transmittanceTexture, SamplerState linearSampler,
    float r, float muS)
{
    const float alphaS = AtmosphereSunAngularRadiusRad(params);
    const float averageCosineFactor = (muS < -alphaS) ? 0.0f
        : ((muS > alphaS) ? muS : (muS + alphaS) * (muS + alphaS) / (4.0f * alphaS));
    return AtmosphereSolarIrradiance(params)
        * AtmosphereSampleTransmittanceToTopBoundary(params, transmittanceTexture, linearSampler, r, muS)
        * averageCosineFactor;
}

float3 AtmosphereComputeIndirectIrradiance(
    FAtmosphereParams params, Texture3D<float4> singleRayleighScatteringTexture,
    Texture3D<float4> singleMieScatteringTexture, Texture3D<float4> multipleScatteringTexture,
    SamplerState linearSampler, float r, float muS, uint scatteringOrder)
{
    const int sampleCount = 32;
    const float dPhi = AE_ATMOSPHERE_PI / float(sampleCount);
    const float dTheta = AE_ATMOSPHERE_PI / float(sampleCount);
    float3 result = 0.0f.xxx;
    const float3 omegaS = float3(sqrt(max(1.0f - muS * muS, 0.0f)), 0.0f, muS);

    [loop]
    for (int j = 0; j < sampleCount / 2; ++j)
    {
        const float theta = (float(j) + 0.5f) * dTheta;
        [loop]
        for (int i = 0; i < 2 * sampleCount; ++i)
        {
            const float phi = (float(i) + 0.5f) * dPhi;
            const float3 omega = float3(cos(phi) * sin(theta), sin(phi) * sin(theta), cos(theta));
            const float domega = dTheta * dPhi * sin(theta);
            const float nu = dot(omega, omegaS);
            result += AtmosphereGetScatteringByOrder(params, singleRayleighScatteringTexture,
                singleMieScatteringTexture, multipleScatteringTexture, linearSampler, r, omega.z,
                muS, nu, false, scatteringOrder) * omega.z * domega;
        }
    }
    return result;
}

float3 AtmosphereComputeScatteringDensity(
    FAtmosphereParams params, Texture2D<float4> transmittanceTexture,
    Texture3D<float4> singleRayleighScatteringTexture,
    Texture3D<float4> singleMieScatteringTexture, Texture3D<float4> multipleScatteringTexture,
    Texture2D<float4> irradianceTexture, SamplerState linearSampler,
    float r, float mu, float muS, float nu, uint scatteringOrder)
{
    const float3 zenithDirection = float3(0.0f, 0.0f, 1.0f);
    const float3 omega = float3(sqrt(max(1.0f - mu * mu, 0.0f)), 0.0f, mu);
    const float sunDirX = (omega.x == 0.0f) ? 0.0f : (nu - mu * muS) / omega.x;
    const float sunDirY = sqrt(max(1.0f - sunDirX * sunDirX - muS * muS, 0.0f));
    const float3 omegaS = float3(sunDirX, sunDirY, muS);

    const int sampleCount = 16;
    const float dPhi = AE_ATMOSPHERE_PI / float(sampleCount);
    const float dTheta = AE_ATMOSPHERE_PI / float(sampleCount);
    float3 rayleighMie = 0.0f.xxx;

    [loop]
    for (int l = 0; l < sampleCount; ++l)
    {
        const float theta = (float(l) + 0.5f) * dTheta;
        const float cosTheta = cos(theta);
        const float sinTheta = sin(theta);
        const bool rayThetaIntersectsGround = AtmosphereRayIntersectsGround(params, r, cosTheta);

        float distanceToGround = 0.0f;
        float3 transmittanceToGround = 0.0f.xxx;
        float3 groundAlbedo = 0.0f.xxx;
        if (rayThetaIntersectsGround)
        {
            distanceToGround = AtmosphereDistanceToBottomBoundary(params, r, cosTheta);
            transmittanceToGround = AtmosphereGetTransmittance(params, transmittanceTexture,
                linearSampler, r, cosTheta, distanceToGround, true);
            groundAlbedo = AtmosphereGroundAlbedo(params);
        }

        [loop]
        for (int m = 0; m < 2 * sampleCount; ++m)
        {
            const float phi = (float(m) + 0.5f) * dPhi;
            const float3 omegaI = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
            const float dOmegaI = dTheta * dPhi * sin(theta);

            const float nu1 = dot(omegaS, omegaI);
            float3 incidentRadiance = AtmosphereGetScatteringByOrder(params,
                singleRayleighScatteringTexture, singleMieScatteringTexture,
                multipleScatteringTexture, linearSampler, r, omegaI.z, muS, nu1,
                rayThetaIntersectsGround, scatteringOrder - 1u);

            const float3 groundNormal =
                normalize(zenithDirection * r + omegaI * distanceToGround);
            const float3 groundIrradiance = AtmosphereGetIrradiance(params, irradianceTexture,
                linearSampler, AtmosphereBottomRadius(params), dot(groundNormal, omegaS));
            incidentRadiance += transmittanceToGround * groundAlbedo
                * (1.0f / AE_ATMOSPHERE_PI) * groundIrradiance;

            const float nu2 = dot(omega, omegaI);
            const float rayleighDensity =
                AtmosphereGetRayleighDensity(params, max(r - AtmosphereBottomRadius(params), 0.0f));
            const float mieDensity =
                AtmosphereGetMieDensity(params, max(r - AtmosphereBottomRadius(params), 0.0f));
            rayleighMie += incidentRadiance * (
                AtmosphereRayleighScattering(params) * rayleighDensity
                    * AtmosphereRayleighPhaseFunction(nu2)
                + AtmosphereMieScattering(params) * mieDensity
                    * AtmosphereMiePhaseFunction(saturate(AtmosphereMieAnisotropy(params)), nu2))
                * dOmegaI;
        }
    }

    return rayleighMie;
}

float3 AtmosphereComputeMultipleScattering(
    FAtmosphereParams params, Texture2D<float4> transmittanceTexture,
    Texture3D<float4> scatteringDensityTexture, SamplerState linearSampler,
    float r, float mu, float muS, float nu, bool rayIntersectsGround)
{
    const int sampleCount = 50;
    const float dx =
        AtmosphereDistanceToNearestBoundary(params, r, mu, rayIntersectsGround) / float(sampleCount);
    float3 rayleighMieSum = 0.0f.xxx;
    [loop]
    for (int i = 0; i <= sampleCount; ++i)
    {
        const float d = float(i) * dx;
        const float rI = AtmosphereClampRadius(params, sqrt(d * d + 2.0f * r * mu * d + r * r));
        const float muI = AtmosphereClampCosine((r * mu + d) / max(rI, 1.0f));
        const float muSI = AtmosphereClampCosine((r * muS + d * nu) / max(rI, 1.0f));
        const float3 rayleighMieI = AtmosphereGetScattering(params, scatteringDensityTexture,
                linearSampler, rI, muI, muSI, nu, rayIntersectsGround)
            * AtmosphereGetTransmittance(
                params, transmittanceTexture, linearSampler, r, mu, d, rayIntersectsGround)
            * dx;
        const float weight = (i == 0 || i == sampleCount) ? 0.5f : 1.0f;
        rayleighMieSum += rayleighMieI * weight;
    }
    return rayleighMieSum;
}
