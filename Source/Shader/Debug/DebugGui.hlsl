cbuffer DebugGuiConstants : register(b0)
{
    float2 gScale;     // 2/width, 2/height
    float2 gTranslate; // -1, -1
    float gSdfEdge;
    float gSdfSoftness;
    float gSdfPixelRange;
    float gAtlasWidth;
    float gAtlasHeight;
    float gUseSdf;
    float gFontStretchX;
    float gGlyphTexelW;
    float gGlyphTexelH;
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
    float2 sampleUv = input.UV;
    if (gUseSdf >= 0.5)
    {
        float2 cellCount = float2(gAtlasWidth / gGlyphTexelW, gAtlasHeight / gGlyphTexelH);
        float2 uvCell = input.UV * cellCount;
        float2 cellBase = floor(uvCell);
        float2 cellLocal = frac(uvCell);
        float stretchX = max(gFontStretchX, 0.01);
        cellLocal.x = saturate((cellLocal.x - 0.5) / stretchX + 0.5);
        float2 cellInset = 0.5 / max(float2(gGlyphTexelW, gGlyphTexelH), float2(1.0, 1.0));
        cellLocal = clamp(cellLocal, cellInset, 1.0 - cellInset);
        sampleUv = (cellBase + cellLocal) / cellCount;
    }

    float4 tex = gFontAtlas.Sample(gSampler, sampleUv);
    if (gUseSdf < 0.5)
    {
        return tex * input.Color;
    }

    float msdf = max(min(tex.r, tex.g), min(max(tex.r, tex.g), tex.b));
    float edge = saturate(gSdfEdge);
    float2 atlasTexelSize = max(float2(gAtlasWidth, gAtlasHeight), float2(1.0, 1.0));
    float2 unitRange = float2(gSdfPixelRange, gSdfPixelRange) / atlasTexelSize;
    float2 uvFwidth = max(fwidth(sampleUv), float2(1e-6, 1e-6));
    float2 screenTexSize = 1.0 / uvFwidth;
    float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);
    float softness = gSdfSoftness * screenPxRange;
    float screenPxDistance = (msdf - edge) * screenPxRange;
    float alpha = saturate(screenPxDistance + 0.5 + softness);
    return float4(input.Color.rgb, input.Color.a * alpha);
}
