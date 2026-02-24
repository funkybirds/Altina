// Simple passthrough blit pixel shader.

#include "Shader/PostProcess/Common.hlsli"

cbuffer BlitConstants : register(b0)
{
    float4 _BlitDummy;
};

float4 PSBlit(FSQOutput input) : SV_Target0
{
    return SceneColor.Sample(LinearSampler, input.UV);
}
