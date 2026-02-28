// @altina raster_state {
//     cull = none;
// }

#include "Shader/Bindings/ShaderBindings.hlsli"

#ifndef AE_PBR_STANDARD_SHARED
#define AE_PBR_STANDARD_SHARED

static const float AE_PBR_PI = 3.1415926535f;

struct FPbrGBufferData
{
    float3 BaseColor;
    float  Metallic;
    float3 Normal;
    float  Roughness;
    float3 Emissive;
    float  Occlusion;
};

FPbrGBufferData DecodePbrGBuffer(float4 gbufferA, float4 gbufferB, float4 gbufferC)
{
    FPbrGBufferData data;
    data.BaseColor = gbufferA.rgb;
    data.Metallic  = gbufferA.a;
    data.Normal    = normalize(gbufferB.xyz * 2.0f - 1.0f);
    data.Roughness = gbufferB.a;
    data.Emissive  = gbufferC.rgb;
    data.Occlusion = gbufferC.a;
    return data;
}

float3 EvaluatePbrDirect(
    const FPbrGBufferData data, float3 N, float3 V, float3 L, float3 lightColor);

float3 EvaluateDeferredPbr(const FPbrGBufferData data)
{
    float3 N = normalize(data.Normal);
    float3 V = float3(0.0f, 0.0f, 1.0f);
    float3 L = normalize(float3(0.4f, 0.6f, 0.7f));
    return EvaluatePbrDirect(data, N, V, L, float3(1.0f, 1.0f, 1.0f));
}

float3 EvaluatePbrDirect(
    const FPbrGBufferData data, float3 N, float3 V, float3 L, float3 lightColor)
{
    N = normalize(N);
    V = normalize(V);
    L = normalize(L);

    const float3 H = normalize(V + L);

    const float NdotL = saturate(dot(N, L));
    const float NdotV = saturate(dot(N, V));
    const float NdotH = saturate(dot(N, H));
    const float VdotH = saturate(dot(V, H));

    const float rough = max(data.Roughness, 0.04f);
    const float a     = rough * rough;
    const float a2    = a * a;
    const float denom = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;
    const float D     = a2 / max(AE_PBR_PI * denom * denom, 1e-4f);

    float k    = (rough + 1.0f);
    k          = (k * k) * 0.125f;
    const float Gv   = NdotV / max(NdotV * (1.0f - k) + k, 1e-4f);
    const float Gl   = NdotL / max(NdotL * (1.0f - k) + k, 1e-4f);
    const float G    = Gv * Gl;

    const float3 F0  = lerp(float3(0.04f, 0.04f, 0.04f), data.BaseColor, data.Metallic);
    const float3 F   = F0 + (1.0f - F0) * pow(1.0f - VdotH, 5.0f);

    const float3 spec = (D * G) * F / max(4.0f * NdotL * NdotV, 1e-4f);
    const float3 kd   = (1.0f - F) * (1.0f - data.Metallic);
    const float3 diff = kd * data.BaseColor / AE_PBR_PI;

    float3 color = (diff + spec) * lightColor * NdotL;
    return color;
}

#endif // AE_PBR_STANDARD_SHARED

#ifndef AE_PBR_STANDARD_NO_CBUFFERS

AE_PER_FRAME_CBUFFER(ViewConstants)
{
    row_major float4x4 ViewProjection;
};

AE_PER_DRAW_CBUFFER(ObjectConstants)
{
    row_major float4x4 World;
    row_major float4x4 NormalMatrix;
};

AE_PER_MATERIAL_CBUFFER(MaterialConstants)
{
    float4 BaseColor;
    float  Metallic;
    float  Roughness;
    float  Occlusion;
    float3 Emissive;
    float  EmissiveIntensity;
    // 0 = ignore NormalTex (use vertex normal), 1 = apply normal map.
    float  NormalMapStrength;
    float3 _MaterialPadding0;
};

Texture2D    BaseColorTex : register(t0);
SamplerState BaseColorTexSampler : register(s0);
Texture2D    NormalTex : register(t1);
SamplerState NormalTexSampler : register(s1);
Texture2D    MetallicTex : register(t2);
SamplerState MetallicTexSampler : register(s2);
Texture2D    RoughnessTex : register(t3);
SamplerState RoughnessTexSampler : register(s3);
Texture2D    EmissiveTex : register(t4);
SamplerState EmissiveTexSampler : register(s4);
Texture2D    OcclusionTex : register(t5);
SamplerState OcclusionTexSampler : register(s5);
Texture2D    SpecularTex : register(t6);
SamplerState SpecularTexSampler : register(s6);
Texture2D    DisplacementTex : register(t7);
SamplerState DisplacementTexSampler : register(s7);

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD0;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float3 Normal   : TEXCOORD0;
    float2 TexCoord : TEXCOORD1;
    float3 WorldPos : TEXCOORD2;
};

VSOutput VSBase(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(World, float4(input.Position, 1.0f));
    output.Position = mul(ViewProjection, worldPos);
    output.Normal   = normalize(mul((float3x3)NormalMatrix, input.Normal));
    output.TexCoord = input.TexCoord;
    output.WorldPos = worldPos.xyz;
    return output;
}

struct PSOutput
{
    float4 GBufferA : SV_Target0;
    float4 GBufferB : SV_Target1;
    float4 GBufferC : SV_Target2;
};

PSOutput PSBase(VSOutput input)
{
    PSOutput output;

    float4 baseColorSample = BaseColorTex.Sample(BaseColorTexSampler, input.TexCoord);
    float3 baseColor       = BaseColor.rgb * baseColorSample.rgb;

    float3 normalWorld = normalize(input.Normal);
    if (NormalMapStrength > 1e-3f)
    {
        const float3 normalTS = normalize(NormalTex.Sample(NormalTexSampler, input.TexCoord).xyz
            * 2.0f - 1.0f);

        const float3 dp1  = ddx(input.WorldPos);
        const float3 dp2  = ddy(input.WorldPos);
        const float2 duv1 = ddx(input.TexCoord);
        const float2 duv2 = ddy(input.TexCoord);

        float3 T = dp1 * duv2.y - dp2 * duv1.y;
        T = (dot(T, T) > 1e-8f) ? normalize(T) : float3(1.0f, 0.0f, 0.0f);
        T = normalize(T - normalWorld * dot(normalWorld, T));
        float3 B = cross(normalWorld, T);

        const float3 normalMapped =
            normalize(T * normalTS.x + B * normalTS.y + normalWorld * normalTS.z);
        normalWorld = normalize(lerp(normalWorld, normalMapped, saturate(NormalMapStrength)));
    }

    float metallic  = saturate(Metallic * MetallicTex.Sample(MetallicTexSampler, input.TexCoord).r);
    float roughness = saturate(Roughness
        * RoughnessTex.Sample(RoughnessTexSampler, input.TexCoord).r);
    float occlusion = saturate(Occlusion
        * OcclusionTex.Sample(OcclusionTexSampler, input.TexCoord).r);
    float specular  = saturate(SpecularTex.Sample(SpecularTexSampler, input.TexCoord).r);
    float displacement =
        saturate(DisplacementTex.Sample(DisplacementTexSampler, input.TexCoord).r);

    metallic  = saturate(metallic + specular * 0.5f);
    roughness = saturate(roughness + displacement * 0.15f);

    float3 emissive =
        Emissive * EmissiveIntensity
        + EmissiveTex.Sample(EmissiveTexSampler, input.TexCoord).rgb * EmissiveIntensity;

    output.GBufferA = float4(baseColor, metallic);
    output.GBufferB = float4(normalWorld * 0.5f + 0.5f, roughness);
    output.GBufferC = float4(emissive, occlusion);
    return output;
}

#endif // AE_PBR_STANDARD_NO_CBUFFERS
