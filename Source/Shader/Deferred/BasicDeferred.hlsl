// @altina raster_state {
//     cull = none;
// }

#include "Shader/Bindings/ShaderBindings.hlsli"

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
};

struct VSInput
{
    float3 Position : POSITION;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float4 Albedo   : COLOR0;
};

VSOutput VSBase(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(World, float4(input.Position, 1.0f));
    output.Position = mul(ViewProjection, worldPos);
    output.Albedo   = BaseColor;
    return output;
}

struct PSOutput
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
};

PSOutput PSBase(VSOutput input)
{
    PSOutput output;
    output.Albedo = input.Albedo;
    output.Normal = float4(0.5f, 0.5f, 1.0f, 1.0f);
    return output;
}

Texture2D    GBufferA : register(t0);
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
    return GBufferA.Sample(GBufferSampler, input.UV);
}
