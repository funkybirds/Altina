# DebugGui MSDF Font Generator

`GenerateDebugGuiMsdf.py` is an offline prototype tool that parses TrueType `glyf` outlines and emits a baked 32x32 ASCII MSDF atlas for DebugGui.

## Usage

```powershell
py -3 Tool/Font/GenerateDebugGuiMsdf.py `
  --ttf Misc/CASCADIAMONO.TTF `
  --out Source/Runtime/DebugGui/Private/DebugGui/FontAtlasMSDF64x64.inl `
  --debug-grid-out Temp/DebugGuiMsdfGrid.png
```

## Current scope

- TrueType `glyf` fonts (no CFF/CFF2)
- `cmap` format 4
- ASCII `32..126`
- Composite glyphs with XY translation and basic scale transforms

The generated output is intended for DebugGui only.
