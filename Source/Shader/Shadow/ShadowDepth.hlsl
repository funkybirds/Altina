// @altina raster_state {
//     cull = front;
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

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD0;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
};

VSOutput VSShadowDepth(VSInput input)
{
    VSOutput output;
    const float4 worldPos = mul(World, float4(input.Position, 1.0f));
    output.Position       = mul(ViewProjection, worldPos);
    return output;
}

float4 PSShadowDepth(VSOutput input) : SV_Target0
{
    //(void)input;
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
