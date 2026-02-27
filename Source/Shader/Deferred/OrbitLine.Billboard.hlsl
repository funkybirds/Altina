// @altina raster_state {
//     cull = none;
// }

#include "Shader/Bindings/ShaderBindings.hlsli"

// This shader renders a world-space polyline as screen-space constant-width quads.
// Geometry is provided as per-segment quads (two endpoints + side sign).
// Depth testing is enabled by the material pass state, so orbit lines can be occluded by planets.

#define AE_MAX_POINT_LIGHTS 16

AE_PER_FRAME_CBUFFER(ViewConstants)
{
    // IMPORTANT:
    // Backed by Rendering::BasicDeferredRenderer::FPerFrameConstants.
    // ViewProjection must remain the first member for layout compatibility.
    row_major float4x4 ViewProjection;

    // Keep layout compatible with the engine's shared constants up to RenderTargetSize.
    row_major float4x4 View;
    row_major float4x4 Proj;
    row_major float4x4 ViewProj;
    row_major float4x4 InvViewProj;

    float3   ViewOriginWS;
    uint     PointLightCount;

    float3   DirLightDirectionWS;
    float    DirLightIntensity;
    float3   DirLightColor;
    uint     CSMCascadeCount;

    row_major float4x4 CSM_LightViewProj0;
    row_major float4x4 CSM_LightViewProj1;
    row_major float4x4 CSM_LightViewProj2;
    row_major float4x4 CSM_LightViewProj3;
    float4   CSM_SplitsVS[4];

    float2   RenderTargetSize;
    float2   InvRenderTargetSize;

    uint     bReverseZ;
    uint     DebugShadingMode;
    float    ShadowBias;
    float    _pad0;
    float2   ShadowMapInvSize;
    float2   _pad1;

    struct FPointLight
    {
        float3 PositionWS;
        float  Range;
        float3 Color;
        float  Intensity;
    };
    FPointLight PointLights[AE_MAX_POINT_LIGHTS];
};

AE_PER_DRAW_CBUFFER(ObjectConstants)
{
    row_major float4x4 World;
    row_major float4x4 NormalMatrix; // Unused here, but kept for layout compatibility.
};

AE_PER_MATERIAL_CBUFFER(MaterialConstants)
{
    float4 BaseColor;
    float  Metallic;
    float  Roughness;
    float  Occlusion;
    float3 Emissive;
    float  EmissiveIntensity;

    // Screen-space constant width in pixels.
    float  LineWidthPx;
    float3 _MaterialPadding0;
};

struct VSInput
{
    float3 Position : POSITION; // Endpoint position (object space).
    float3 OtherPos : NORMAL;   // The other endpoint position (object space).
    float2 Params   : TEXCOORD0; // x = sideSign (-1/+1), y = endpointFlag (0/1)
};

struct VSOutput
{
    float4 Position : SV_POSITION;
};

VSOutput VSBase(VSInput input)
{
    VSOutput output;

    const float4 pWS = mul(World, float4(input.Position, 1.0f));
    const float4 qWS = mul(World, float4(input.OtherPos, 1.0f));

    const float4 pClip = mul(ViewProjection, pWS);
    const float4 qClip = mul(ViewProjection, qWS);

    // Convert to NDC for computing a screen-space perpendicular.
    const float2 pNdc = pClip.xy / max(pClip.w, 1e-6f);
    const float2 qNdc = qClip.xy / max(qClip.w, 1e-6f);

    float2 dir = qNdc - pNdc;
    const float dirLen2 = dot(dir, dir);
    dir = (dirLen2 > 1e-12f) ? (dir * rsqrt(dirLen2)) : float2(1.0f, 0.0f);

    const float2 perp = float2(-dir.y, dir.x);

    // Pixel -> NDC conversion ([-1,1] range).
    float2 invRt = InvRenderTargetSize;
    // Defensive fallback: if the per-frame constants layout ever diverges, keep the debug orbit
    // lines visible rather than silently collapsing to zero width.
    if (invRt.x <= 0.0f || invRt.y <= 0.0f) {
        invRt = float2(1.0f / 1920.0f, 1.0f / 1080.0f);
    }

    const float2 pixelToNdc  = float2(2.0f, 2.0f) * invRt;
    const float  halfWidthPx = max(LineWidthPx, 2.0f) * 0.5f;

    const float  side = input.Params.x; // -1 / +1
    float2 offsetNdc = perp * (side * halfWidthPx) * pixelToNdc;

    // Extend each segment slightly beyond its endpoints (in screen space) so adjacent segments
    // overlap. This reduces small cracks/"dashed" artifacts on curved polylines where each segment
    // computes its own screen-space perpendicular.
    const float endFlag = saturate(input.Params.y); // 0 at p0, 1 at p1
    const float endSign = (endFlag * 2.0f - 1.0f);  // -1 at p0, +1 at p1
    offsetNdc += dir * (endSign * halfWidthPx) * pixelToNdc;

    // Apply offset in clip space: clip.xy = (ndc.xy + offsetNdc) * w
    float4 clip = pClip;
    clip.xy += offsetNdc * pClip.w;

    output.Position = clip;
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

    // Unlit-ish: rely on emissive so the orbit line stays readable.
    const float3 normalWorld = float3(0.0f, 1.0f, 0.0f);

    output.GBufferA = float4(BaseColor.rgb, saturate(Metallic));
    output.GBufferB = float4(normalWorld * 0.5f + 0.5f, saturate(Roughness));
    output.GBufferC = float4(Emissive * EmissiveIntensity, saturate(Occlusion));
    return output;
}
