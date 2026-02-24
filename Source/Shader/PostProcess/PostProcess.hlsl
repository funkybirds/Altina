// Deprecated legacy entry point file.
//
// The engine compiles built-in post-process shaders from individual source files now:
// - FullscreenTriangle.hlsl (VSFullScreenTriangle)
// - Blit.hlsl              (PSBlit)
// - Tonemap.hlsl           (PSTonemap)
// - Fxaa.hlsl              (PSFxaa)
// - Bloom.hlsl             (PSBloom*)
//
// This file intentionally does not include them because each pass now owns its own constant buffer
// (b0), and including multiple passes in a single translation unit would cause register conflicts.
