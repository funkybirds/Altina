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

#if defined(AE_SHADER_TARGET_VULKAN)
    #define AE_PBR_REG_VIEW      register(b0, space0)
    #define AE_PBR_REG_OBJECT    register(t0, space1)
    #define AE_PBR_REG_MATERIAL  register(b0, space2)
    #define AE_PBR_REG_T0        register(t0, space2)
    #define AE_PBR_REG_T1        register(t1, space2)
    #define AE_PBR_REG_T2        register(t2, space2)
    #define AE_PBR_REG_T3        register(t3, space2)
    #define AE_PBR_REG_T4        register(t4, space2)
    #define AE_PBR_REG_T5        register(t5, space2)
    #define AE_PBR_REG_T6        register(t6, space2)
    #define AE_PBR_REG_T7        register(t7, space2)
    #define AE_PBR_REG_S0        register(s0, space2)
    #define AE_PBR_REG_S1        register(s1, space2)
    #define AE_PBR_REG_S2        register(s2, space2)
    #define AE_PBR_REG_S3        register(s3, space2)
    #define AE_PBR_REG_S4        register(s4, space2)
    #define AE_PBR_REG_S5        register(s5, space2)
    #define AE_PBR_REG_S6        register(s6, space2)
    #define AE_PBR_REG_S7        register(s7, space2)
#else
    #define AE_PBR_REG_VIEW      register(b0)
    #define AE_PBR_REG_OBJECT    register(t4)
    #define AE_PBR_REG_MATERIAL  register(b8)
    #define AE_PBR_REG_T0        register(t32)
    #define AE_PBR_REG_T1        register(t33)
    #define AE_PBR_REG_T2        register(t34)
    #define AE_PBR_REG_T3        register(t35)
    #define AE_PBR_REG_T4        register(t36)
    #define AE_PBR_REG_T5        register(t37)
    #define AE_PBR_REG_T6        register(t38)
    #define AE_PBR_REG_T7        register(t39)
    #define AE_PBR_REG_S0        register(s8)
    #define AE_PBR_REG_S1        register(s9)
    #define AE_PBR_REG_S2        register(s10)
    #define AE_PBR_REG_S3        register(s11)
    #define AE_PBR_REG_S4        register(s12)
    #define AE_PBR_REG_S5        register(s13)
    #define AE_PBR_REG_S6        register(s14)
    #define AE_PBR_REG_S7        register(s15)
#endif

cbuffer ViewConstants : AE_PBR_REG_VIEW
{
    row_major float4x4 ViewProjection;
};

ByteAddressBuffer InstanceDataBuffer : AE_PBR_REG_OBJECT;

cbuffer MaterialConstants : AE_PBR_REG_MATERIAL
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

Texture2D    BaseColorTex         : AE_PBR_REG_T0;
SamplerState BaseColorTexSampler  : AE_PBR_REG_S0;
Texture2D    NormalTex            : AE_PBR_REG_T1;
SamplerState NormalTexSampler     : AE_PBR_REG_S1;
Texture2D    MetallicTex          : AE_PBR_REG_T2;
SamplerState MetallicTexSampler   : AE_PBR_REG_S2;
Texture2D    RoughnessTex         : AE_PBR_REG_T3;
SamplerState RoughnessTexSampler  : AE_PBR_REG_S3;
Texture2D    EmissiveTex          : AE_PBR_REG_T4;
SamplerState EmissiveTexSampler   : AE_PBR_REG_S4;
Texture2D    OcclusionTex         : AE_PBR_REG_T5;
SamplerState OcclusionTexSampler  : AE_PBR_REG_S5;
Texture2D    SpecularTex          : AE_PBR_REG_T6;
SamplerState SpecularTexSampler   : AE_PBR_REG_S6;
Texture2D    DisplacementTex      : AE_PBR_REG_T7;
SamplerState DisplacementTexSampler : AE_PBR_REG_S7;

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

static const uint AE_INSTANCE_MATRIX_SIZE_BYTES = 64u;
static const uint AE_INSTANCE_STRIDE_BYTES      = 128u;

float4 LoadInstanceFloat4(uint byteOffset)
{
    return asfloat(InstanceDataBuffer.Load4(byteOffset));
}

void LoadInstanceTransforms(
    uint instanceId, out row_major float4x4 world, out row_major float4x4 normalMatrix)
{
    const uint baseOffset = instanceId * AE_INSTANCE_STRIDE_BYTES;
    world[0] = LoadInstanceFloat4(baseOffset + 0u);
    world[1] = LoadInstanceFloat4(baseOffset + 16u);
    world[2] = LoadInstanceFloat4(baseOffset + 32u);
    world[3] = LoadInstanceFloat4(baseOffset + 48u);

    normalMatrix[0] = LoadInstanceFloat4(baseOffset + AE_INSTANCE_MATRIX_SIZE_BYTES + 0u);
    normalMatrix[1] = LoadInstanceFloat4(baseOffset + AE_INSTANCE_MATRIX_SIZE_BYTES + 16u);
    normalMatrix[2] = LoadInstanceFloat4(baseOffset + AE_INSTANCE_MATRIX_SIZE_BYTES + 32u);
    normalMatrix[3] = LoadInstanceFloat4(baseOffset + AE_INSTANCE_MATRIX_SIZE_BYTES + 48u);
}

VSOutput VSBase(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;
    row_major float4x4 World;
    row_major float4x4 NormalMatrix;
    LoadInstanceTransforms(instanceId, World, NormalMatrix);
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
