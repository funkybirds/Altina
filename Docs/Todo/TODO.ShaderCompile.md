# ShaderCompile Module TODO

This document captures the proposed architecture for a ShaderCompile module that targets
D3D12, Vulkan, and D3D11, with hot-reload support. It is intentionally implementation-lean
and focuses on module boundaries, data flow, and integration points.

## Goals
- Single HLSL/Slang-style source of truth with per-platform outputs.
- Auto resource binding for Vulkan (no manual `set/binding` in source).
- Unified bindless and non-bindless shader access patterns.
- Hot compilation during editor/development runs.
- Stable, cacheable compile outputs with reflection metadata.

## Non-Goals (for first milestone)
- Full shader graph / material editor.
- Runtime shader compilation on shipping builds.
- Full cross-platform binary compatibility (mobile/console targets).

## Module Placement (Engine/Domain)
```
Source/Engine/ShaderCompiler/
  Public/
    ShaderCompiler/ShaderCompiler.h
    ShaderCompiler/ShaderCompileTypes.h
    ShaderCompiler/ShaderReflection.h
    ShaderCompiler/ShaderPermutation.h
  Private/
    ShaderCompiler/ShaderCompiler.cpp
    ShaderCompiler/ShaderCompilerBackend.h
    ShaderCompiler/DxcCompiler.cpp
    ShaderCompiler/SlangCompiler.cpp
    ShaderCompiler/FxcCompiler.cpp
    ShaderCompiler/ShaderBindingRules.cpp
    ShaderCompiler/ShaderCache.cpp
```

Notes:
- This module is engine-level (not RHI), but outputs RHI-usable data.
- Platform-specific compiler wrappers are private.
- Add a small JSON/TOML manifest later if build metadata becomes data-driven.

## Public API (Draft)
Key types to expose in `Public/ShaderCompiler/ShaderCompileTypes.h`:
- `FShaderSourceDesc`
  - `FString mPath`
  - `FString mEntryPoint`
  - `ERhiShaderStage mStage`
  - `EShaderSourceLanguage mLanguage` (HLSL, Slang)
  - `TArray<FString> mIncludeDirs`
  - `TArray<FShaderMacro> mDefines`
- `FShaderCompileOptions`
  - `ERhiBackend mTargetBackend` (D3D12, Vulkan, D3D11)
  - `EShaderOptimization mOptimization`
  - `bool mDebugInfo`
  - `bool mEnableBindless`
  - `FString mTargetProfile` (e.g. `vs_6_6`, `ps_6_6`, `cs_6_6`)
  - `FString mCompilerPathOverride`
  - `FString mShaderModelOverride`
- `FShaderCompileRequest`
  - `FShaderSourceDesc mSource`
  - `FShaderCompileOptions mOptions`
  - `FShaderPermutationId mPermutationId`
- `FShaderCompileResult`
  - `bool mSucceeded`
  - `TArray<u8> mBytecode`
  - `FShaderReflection mReflection`
  - `FString mDiagnostics`
  - `FString mOutputDebugPath`

Primary entry points in `Public/ShaderCompiler/ShaderCompiler.h`:
- `class IShaderCompiler`
  - `virtual FShaderCompileResult Compile(const FShaderCompileRequest&) = 0;`
  - `virtual void CompileAsync(const FShaderCompileRequest&, FOnShaderCompiled) = 0;`
- `IShaderCompiler& GetShaderCompiler();`

## Compile Pipeline (Logical Stages)
1. **Preprocess**: resolve `#include` and apply `mDefines`.
2. **Compile**: Slang/DXC/FXC produce platform bytecode.
3. **Reflect**: extract resources, entry point IO, push constants, etc.
4. **Auto-Bind**: assign `set/binding` (Vulkan) or `space/register` (D3D12).
5. **Emit Metadata**: serialize reflection and binding tables.
6. **Cache**: hash-based reuse across builds.

## Compiler Backends
- **Slang**: preferred for HLSL-like syntax + cross-target backends.
  - Outputs DXIL (D3D12) and SPIR-V (Vulkan).
- **DXC**: HLSL -> DXIL/SPIR-V (when Slang not available or for parity tests).
- **FXC**: legacy D3D11 path (DXBC) when D3D11 requires compatibility.

Backend selection rules:
- If `mLanguage == Slang` and Slang available, use Slang.
- If `mLanguage == HLSL`, prefer DXC; fallback to FXC for D3D11 DXBC.
- Allow per-target override in `FShaderCompileOptions`.

## Resource Binding Strategy
### Goal
Avoid explicit `set/binding` in shader source while supporting Vulkan rules.

### Approach
- Parse reflected resources and assign bindings by **usage group**:
  - `PerFrame` -> set 0
  - `PerPass` -> set 1
  - `PerMaterial` -> set 2
  - `Bindless` -> set 3 (global array tables)
  - `Local` / `Inline` -> push constants or root constants
- The mapping is a deterministic rule-set in `ShaderBindingRules.cpp`.
- D3D12 uses `space` to mirror Vulkan set.
- D3D11 ignores set; uses a compact register allocation.

### Tagging in Source
Two lightweight options:
- Comment tags: `// @set(PerMaterial)`
- Macros: `AE_RESOURCE(PerMaterial, Texture2D, Albedo)`

If no tag exists, default group is `PerMaterial`.

## Bindless vs Non-Bindless
Design to compile the same source with a macro:
- `AE_BINDLESS=1` -> global descriptor arrays, resource access via handle index.
- `AE_BINDLESS=0` -> arrays with fixed small limits.

Shader code should access through wrapper macros:
```
AE_TEX2D(handle).Sample(AE_SAMP(samp), uv)
```

Reflection emits a `BindlessTable` entry when `AE_BINDLESS=1`.

## Hot Reload
Runtime flow:
- A file watcher (Editor/Tools layer) emits compile requests on change.
- ShaderCompile runs async jobs; on success, publishes a new shader blob.
- RHI recreates pipeline or patches shader in-place when safe.
- On failure, keep the last valid shader and report diagnostics.

Concurrency:
- Compile queue + worker threads (reuse JobSystem).
- Avoid blocking render thread; do a swap on render-safe point.

## Cache and Derived Data
Cache key:
- Shader source hash + include hash + defines + compiler version + options.

Suggested storage:
- `out/ShaderCache/<backend>/<hash>.bin`
- `out/ShaderCache/<backend>/<hash>.meta`

`mOutputDebugPath` should include a stable debug path for tooling.

## Output Formats
1. **Bytecode blob**: DXIL/DXBC/SPIR-V
2. **Metadata** (JSON or binary):
   - Entry point signature (inputs/outputs)
   - Resource list with set/binding/register
   - Push constants / root constants
   - Thread group size (compute)

## Integration Points
- **RHI**: consume `FShaderReflection` to build `FRhiPipelineLayoutDesc`.
- **Assets**: material/shader assets reference compiled bytecode + metadata.
- **Build system**: add `Scripts/BuildShaders.ps1` and `.sh` wrappers.
- **Editor**: hook file watcher and diagnostics UI.

## Testing Plan
- Unit tests for binding rules (deterministic set/binding).
- Golden tests comparing reflection across Slang vs DXC.
- Smoke test compiling a minimal VS/PS/CS for all three backends.

## Milestones (Proposed)
1. **Module skeleton** + minimal public API + DXC compile path.
2. **Reflection** + auto binding rules + metadata output.
3. **Vulkan SPIR-V** path + Slang integration.
4. **D3D11 FXC** compatibility path.
5. **Hot reload** + cache + async compilation.

