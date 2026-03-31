// @altina raster_state {
//     cull = front;
// }

#include "Shader/Bindings/ShaderBindings.hlsli"

AE_PER_FRAME_CBUFFER(ViewConstants)
{
    row_major float4x4 ViewProjection;
};

AE_PER_DRAW_SRV(ByteAddressBuffer, InstanceDataBuffer);

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

static const uint AE_INSTANCE_STRIDE_BYTES = 128u;

float4 LoadInstanceFloat4(uint byteOffset)
{
    return asfloat(InstanceDataBuffer.Load4(byteOffset));
}

void LoadWorldMatrix(uint instanceId, out row_major float4x4 world)
{
    const uint baseOffset = instanceId * AE_INSTANCE_STRIDE_BYTES;
    world[0] = LoadInstanceFloat4(baseOffset + 0u);
    world[1] = LoadInstanceFloat4(baseOffset + 16u);
    world[2] = LoadInstanceFloat4(baseOffset + 32u);
    world[3] = LoadInstanceFloat4(baseOffset + 48u);
}

VSOutput VSShadowDepth(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;
    row_major float4x4 World;
    LoadWorldMatrix(instanceId, World);
    const float4 worldPos = mul(World, float4(input.Position, 1.0f));
    output.Position       = mul(ViewProjection, worldPos);
    return output;
}

float PSShadowDepth(VSOutput input) : SV_Depth
{
    return input.Position.z / input.Position.w;
}
