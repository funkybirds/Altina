// @altina raster_state {
//     cull = front;
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

float3 EvaluateDeferredPbr(const FPbrGBufferData data)
{
    float3 N = normalize(data.Normal);
    float3 V = float3(0.0f, 0.0f, 1.0f);
    float3 L = normalize(float3(0.4f, 0.6f, 0.7f));
    float3 H = normalize(V + L);

    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    float rough = max(data.Roughness, 0.04f);
    float a     = rough * rough;
    float a2    = a * a;
    float denom = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;
    float D     = a2 / max(AE_PBR_PI * denom * denom, 1e-4f);

    float k    = (rough + 1.0f);
    k          = (k * k) * 0.125f;
    float Gv   = NdotV / max(NdotV * (1.0f - k) + k, 1e-4f);
    float Gl   = NdotL / max(NdotL * (1.0f - k) + k, 1e-4f);
    float G    = Gv * Gl;

    float3 F0  = lerp(float3(0.04f, 0.04f, 0.04f), data.BaseColor, data.Metallic);
    float3 F   = F0 + (1.0f - F0) * pow(1.0f - VdotH, 5.0f);

    float3 spec = (D * G) * F / max(4.0f * NdotL * NdotV, 1e-4f);
    float3 kd   = (1.0f - F) * (1.0f - data.Metallic);
    float3 diff = kd * data.BaseColor / AE_PBR_PI;

    const float3 lightColor = float3(1.0f, 1.0f, 1.0f);
    float3       color      = (diff + spec) * lightColor * NdotL;
    color += data.Emissive;
    color *= lerp(0.35f, 1.0f, data.Occlusion);
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
};

AE_PER_MATERIAL_CBUFFER(MaterialConstants)
{
    float4 BaseColor;
    float  Metallic;
    float  Roughness;
    float  Occlusion;
    float3 Emissive;
    float  EmissiveIntensity;
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
};

VSOutput VSBase(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(World, float4(input.Position, 1.0f));
    output.Position = mul(ViewProjection, worldPos);
    output.Normal   = normalize(mul((float3x3)World, input.Normal));
    output.TexCoord = input.TexCoord;
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

    float3 normalSample = NormalTex.Sample(NormalTexSampler, input.TexCoord).xyz * 2.0f - 1.0f;
    float3 normalWorld  = normalize(input.Normal + normalSample);

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
