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

// Keep a pixel shader (some toolchains/backends don't like a "void" PS here), but write depth
// explicitly so we don't need any color RTV bound.
float PSShadowDepth(VSOutput input) : SV_Depth
{
    // SV_Position is in clip-space; SV_Depth expects post-projection depth in [0,1].
    return input.Position.z / input.Position.w;
}
