cbuffer DebugGuiConstants : register(b0)
{
    float2 gScale;     // 2/width, 2/height
    float2 gTranslate; // -1, -1
};

Texture2D gFontAtlas : register(t0);
SamplerState gSampler : register(s0);

struct VSInput
{
    float2 Pos   : POSITION;
    float2 UV    : TEXCOORD0;
    float4 Color : COLOR0;
};

struct VSOutput
{
    float4 Pos   : SV_Position;
    float2 UV    : TEXCOORD0;
    float4 Color : COLOR0;
};

VSOutput DebugGuiVSMain(VSInput input)
{
    VSOutput o;
    float2 ndc = input.Pos * gScale + gTranslate;
    o.Pos = float4(ndc.x, -ndc.y, 0.0, 1.0); // flip Y (top-left origin)
    o.UV = input.UV;
    o.Color = input.Color;
    return o;
}

float4 DebugGuiPSMain(VSOutput input) : SV_Target0
{
    float4 tex = gFontAtlas.Sample(gSampler, input.UV);
    float a = tex.a;
    float4 outColor = input.Color;
    outColor.a *= a;
    return outColor;
}
