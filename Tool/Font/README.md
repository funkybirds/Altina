# DebugGui MSDF Font Generator

`GenerateDebugGuiMsdf.py` is the active DebugGui font atlas generator.

## Usage

```powershell
py -3 Tool/Font/GenerateDebugGuiMsdf.py `
  --ttf Misc/CASCADIAMONO.TTF `
  --out Source/Runtime/DebugGui/Private/DebugGui/FontAtlasMSDF64x64.inl
```

## Preserve existing metrics

If you want to regenerate only the glyph MSDF data while keeping the current text scale, aspect ratio, and stretch metadata:

```powershell
py -3 Tool/Font/GenerateDebugGuiMsdf.py `
  --ttf Misc/CASCADIAMONO.TTF `
  --out Source/Runtime/DebugGui/Private/DebugGui/FontAtlasMSDF64x64.inl `
  --metrics-from Source/Runtime/DebugGui/Private/DebugGui/FontAtlasMSDF64x64.inl
```

## Notes

- ASCII `32..126`
- TrueType `glyf` parsing in Python
- Optional debug atlas preview via `--debug-grid-out`
- `GenerateDebugGuiMsdfReference.py` remains as an alternate third-party path, but the default workflow is `GenerateDebugGuiMsdf.py`
