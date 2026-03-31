// @altina raster_state {
//     cull = none;
// }

#include "Shader/Bindings/ShaderBindings.hlsli"

#define AE_PBR_STANDARD_NO_CBUFFERS
#include "Shader/Deferred/Materials/Lit/PBR.Standard.hlsl"
#undef AE_PBR_STANDARD_NO_CBUFFERS

AE_PER_FRAME_CBUFFER(ViewConstants)
{
    row_major float4x4 ViewProjection;
};

AE_PER_DRAW_SRV(ByteAddressBuffer, InstanceDataBuffer);

AE_PER_MATERIAL_CBUFFER(MaterialConstants)
{
    float4 BaseColor;
    float  Metallic;
    float  Roughness;
    float  Occlusion;
    float3 Emissive;
    float  EmissiveIntensity;
};

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
    output.GBufferA = float4(BaseColor.rgb, Metallic);
    output.GBufferB = float4(input.Normal * 0.5f + 0.5f, Roughness);
    output.GBufferC = float4(Emissive * EmissiveIntensity, Occlusion);
    return output;
}

Texture2D    GBufferA : register(t0);
Texture2D    GBufferB : register(t1);
Texture2D    GBufferC : register(t2);
SamplerState GBufferSampler : register(s0);

struct FSQOutput
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

FSQOutput VSComposite(uint vertexId : SV_VertexID)
{
    FSQOutput output;
    float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };
    float2 uvs[3] =
    {
        float2(0.0f, 1.0f),
        float2(0.0f, -1.0f),
        float2(2.0f, 1.0f)
    };
    output.Position = float4(positions[vertexId], 0.0f, 1.0f);
    output.UV       = uvs[vertexId];
    return output;
}

float4 PSComposite(FSQOutput input) : SV_Target0
{
    const float4 gbufferA = GBufferA.Sample(GBufferSampler, input.UV);
    const float4 gbufferB = GBufferB.Sample(GBufferSampler, input.UV);
    const float4 gbufferC = GBufferC.Sample(GBufferSampler, input.UV);
    const FPbrGBufferData data = DecodePbrGBuffer(gbufferA, gbufferB, gbufferC);
    
    return float4(data.BaseColor, 1.0f);
    //return float4(EvaluateDeferredPbr(data), 1.0f); 2
}
