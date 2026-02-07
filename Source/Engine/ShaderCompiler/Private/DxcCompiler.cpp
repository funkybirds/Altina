#include "ShaderAutoBinding.h"
#include "ShaderCompilerBackend.h"
#include "ShaderCompilerUtils.h"
#include "ShaderCompiler/ShaderRhiBindings.h"
#include "Container/String.h"
#include "Platform/PlatformFileSystem.h"
#include "Platform/PlatformProcess.h"

#include <cstring>
#include <string>

#if AE_PLATFORM_WIN
    #ifdef TEXT
        #undef TEXT
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <unknwn.h>
    #include <objidl.h>
    #include <oleauto.h>
    #include <dxcapi.h>
    #include <wrl/client.h>
    #include <d3d12shader.h>
    #include <d3dcompiler.h>
    #ifdef TEXT
        #undef TEXT
    #endif
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        #define TEXT(str) L##str
    #else
        #define TEXT(str) str
    #endif
#endif

namespace AltinaEngine::ShaderCompiler::Detail {
    using Core::Container::FString;
    using Core::Platform::ReadFileBytes;
    using Core::Platform::RemoveFileIfExists;
    using Core::Platform::RunProcess;

    namespace {
        constexpr const TChar* kDxcName = TEXT("DXC");
        constexpr const TChar* kDxcDisabledMessage =
            TEXT("DXC backend disabled. Define AE_SHADER_COMPILER_ENABLE_DXC=1 to enable.");

        auto GetStageProfileSuffix(EShaderStage stage) -> const TChar* {
            switch (stage) {
            case EShaderStage::Vertex:
                return TEXT("vs");
            case EShaderStage::Pixel:
                return TEXT("ps");
            case EShaderStage::Compute:
                return TEXT("cs");
            case EShaderStage::Geometry:
                return TEXT("gs");
            case EShaderStage::Hull:
                return TEXT("hs");
            case EShaderStage::Domain:
                return TEXT("ds");
            case EShaderStage::Mesh:
                return TEXT("ms");
            case EShaderStage::Amplification:
                return TEXT("as");
            case EShaderStage::Library:
                return TEXT("lib");
            default:
                return TEXT("ps");
            }
        }

        auto BuildDefaultProfile(EShaderStage stage, Rhi::ERhiBackend backend) -> FString {
            FString profile;
            profile.Append(GetStageProfileSuffix(stage));
            profile.Append(TEXT("_"));

            switch (backend) {
            case Rhi::ERhiBackend::DirectX11:
                profile.Append(TEXT("5_0"));
                break;
            case Rhi::ERhiBackend::DirectX12:
            case Rhi::ERhiBackend::Vulkan:
            default:
                profile.Append(TEXT("6_6"));
                break;
            }
            return profile;
        }

        auto GetOptimizationFlag(EShaderOptimization optimization) -> FString {
            switch (optimization) {
            case EShaderOptimization::Debug:
                return FString(TEXT("-O0"));
            case EShaderOptimization::Performance:
                return FString(TEXT("-O3"));
            case EShaderOptimization::Size:
                return FString(TEXT("-O2"));
            case EShaderOptimization::Default:
            default:
                return FString(TEXT("-O1"));
            }
        }

        auto ToFString(u32 value) -> FString {
            const auto text = std::to_string(value);
            FString    out;
            for (char ch : text) {
                out.Append(static_cast<TChar>(ch));
            }
            return out;
        }

        void AddArg(TVector<FString>& args, const TChar* text) {
            args.EmplaceBack(text);
        }

        void AppendVulkanBindingArgs(const FVulkanBindingOptions& options,
            const TVector<u32>* spaces, TVector<FString>& args) {
            if (!options.mEnableAutoShift) {
                return;
            }

            auto AppendShiftForSpace = [&](u32 space) {
                AddArg(args, TEXT("-fvk-b-shift"));
                args.PushBack(ToFString(options.mConstantBufferShift));
                args.PushBack(ToFString(space));

                AddArg(args, TEXT("-fvk-t-shift"));
                args.PushBack(ToFString(options.mTextureShift));
                args.PushBack(ToFString(space));

                AddArg(args, TEXT("-fvk-s-shift"));
                args.PushBack(ToFString(options.mSamplerShift));
                args.PushBack(ToFString(space));

                AddArg(args, TEXT("-fvk-u-shift"));
                args.PushBack(ToFString(options.mStorageShift));
                args.PushBack(ToFString(space));
            };

            if (spaces != nullptr && !spaces->IsEmpty()) {
                for (u32 space : *spaces) {
                    AppendShiftForSpace(space);
                }
            } else {
                AppendShiftForSpace(options.mSpace);
            }
        }

#if AE_PLATFORM_WIN
        auto ConvertNameToString(const char* name) -> FString {
            FString out;
            if (name == nullptr) {
                return out;
            }
            const auto length = std::strlen(name);
            if (length == 0) {
                return out;
            }
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
            int wideCount = MultiByteToWideChar(CP_UTF8, 0, name,
                static_cast<int>(length), nullptr, 0);
            if (wideCount <= 0) {
                return out;
            }
            std::wstring wide(static_cast<size_t>(wideCount), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, name, static_cast<int>(length),
                wide.data(), wideCount);
            out.Append(wide.c_str(), wide.size());
#else
            out.Append(name, length);
#endif
            return out;
        }

        auto MapResourceType(const D3D12_SHADER_INPUT_BIND_DESC& desc,
            EShaderResourceAccess& outAccess) -> EShaderResourceType {
            outAccess = EShaderResourceAccess::ReadOnly;
            switch (desc.Type) {
            case D3D_SIT_CBUFFER:
            case D3D_SIT_TBUFFER:
                return EShaderResourceType::ConstantBuffer;
            case D3D_SIT_SAMPLER:
                return EShaderResourceType::Sampler;
            case D3D_SIT_TEXTURE:
                return EShaderResourceType::Texture;
            case D3D_SIT_STRUCTURED:
            case D3D_SIT_BYTEADDRESS:
                return EShaderResourceType::StorageBuffer;
            case D3D_SIT_UAV_RWTYPED:
            case D3D_SIT_UAV_RWSTRUCTURED:
            case D3D_SIT_UAV_RWBYTEADDRESS:
            case D3D_SIT_UAV_APPEND_STRUCTURED:
            case D3D_SIT_UAV_CONSUME_STRUCTURED:
            case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
            case D3D_SIT_UAV_FEEDBACKTEXTURE:
                outAccess = EShaderResourceAccess::ReadWrite;
                return EShaderResourceType::StorageTexture;
            case D3D_SIT_RTACCELERATIONSTRUCTURE:
                return EShaderResourceType::AccelerationStructure;
            default:
                return EShaderResourceType::Texture;
            }
        }

        auto ExtractReflectionFromDxil(const TVector<u8>& bytecode, FShaderReflection& outReflection,
            FString& diagnostics) -> bool {
            if (bytecode.IsEmpty()) {
                AppendDiagnosticLine(diagnostics, TEXT("DXC reflection: empty bytecode."));
                return false;
            }

            ID3D12ShaderReflection* reflector = nullptr;
            const HRESULT hr = D3DReflect(bytecode.Data(), bytecode.Size(),
                IID_ID3D12ShaderReflection, reinterpret_cast<void**>(&reflector));
            if (FAILED(hr) || reflector == nullptr) {
                using Microsoft::WRL::ComPtr;
                ComPtr<IDxcUtils> utils;
                if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)))) {
                    AppendDiagnosticLine(diagnostics,
                        TEXT("DXC reflection: failed to create IDxcUtils."));
                    return false;
                }

                ComPtr<IDxcContainerReflection> container;
                if (FAILED(DxcCreateInstance(CLSID_DxcContainerReflection,
                        IID_PPV_ARGS(&container)))) {
                    AppendDiagnosticLine(diagnostics,
                        TEXT("DXC reflection: failed to create container reflection."));
                    return false;
                }

                ComPtr<IDxcBlobEncoding> blob;
                if (FAILED(utils->CreateBlobFromPinned(bytecode.Data(),
                        static_cast<UINT32>(bytecode.Size()), 0, &blob))) {
                    AppendDiagnosticLine(diagnostics,
                        TEXT("DXC reflection: failed to create DXIL blob."));
                    return false;
                }

                if (FAILED(container->Load(blob.Get()))) {
                    AppendDiagnosticLine(diagnostics,
                        TEXT("DXC reflection: failed to load DXIL container."));
                    return false;
                }

                UINT32 partIndex = 0;
                if (FAILED(container->FindFirstPartKind(DXC_PART_DXIL, &partIndex))) {
                    AppendDiagnosticLine(diagnostics,
                        TEXT("DXC reflection: DXIL part not found in container."));
                    return false;
                }

                if (FAILED(container->GetPartReflection(partIndex, IID_PPV_ARGS(&reflector)))
                    || reflector == nullptr) {
                    AppendDiagnosticLine(diagnostics,
                        TEXT("DXC reflection: container reflection failed."));
                    return false;
                }
            }

            D3D12_SHADER_DESC desc{};
            reflector->GetDesc(&desc);

            outReflection.mResources.Clear();
            outReflection.mResources.Reserve(desc.BoundResources);

            for (UINT i = 0; i < desc.BoundResources; ++i) {
                D3D12_SHADER_INPUT_BIND_DESC bindDesc{};
                if (FAILED(reflector->GetResourceBindingDesc(i, &bindDesc))) {
                    continue;
                }

                FShaderResourceBinding binding{};
                binding.mName = ConvertNameToString(bindDesc.Name);
                binding.mRegister = bindDesc.BindPoint;
                binding.mSpace    = bindDesc.Space;
                binding.mBinding  = bindDesc.BindPoint;
                binding.mSet      = bindDesc.Space;
                binding.mType     = MapResourceType(bindDesc, binding.mAccess);
                outReflection.mResources.PushBack(binding);
            }

            UINT tgx = 1;
            UINT tgy = 1;
            UINT tgz = 1;
            reflector->GetThreadGroupSize(&tgx, &tgy, &tgz);
            outReflection.mThreadGroupSizeX = tgx;
            outReflection.mThreadGroupSizeY = tgy;
            outReflection.mThreadGroupSizeZ = tgz;

            reflector->Release();
            return true;
        }
#endif

        auto BuildCompilerArgs(const FShaderCompileRequest& request, const FString& outputPath,
            const FString& sourcePath)
            -> TVector<FString> {
            TVector<FString> args;

            if (!request.mSource.mEntryPoint.IsEmptyString()) {
                AddArg(args, TEXT("-E"));
                args.PushBack(request.mSource.mEntryPoint);
            }

            FString profile = request.mOptions.mTargetProfile;
            if (profile.IsEmptyString()) {
                profile = BuildDefaultProfile(request.mSource.mStage, request.mOptions.mTargetBackend);
            }
            AddArg(args, TEXT("-T"));
            args.PushBack(profile);

            AddArg(args, TEXT("-Fo"));
            args.PushBack(outputPath);

            if (request.mOptions.mDebugInfo) {
                AddArg(args, TEXT("-Zi"));
            }

            args.PushBack(GetOptimizationFlag(request.mOptions.mOptimization));

            for (const auto& includeDir : request.mSource.mIncludeDirs) {
                AddArg(args, TEXT("-I"));
                args.PushBack(includeDir);
            }

            for (const auto& macro : request.mSource.mDefines) {
                AddArg(args, TEXT("-D"));
                if (macro.mValue.IsEmptyString()) {
                    args.PushBack(macro.mName);
                } else {
                    FString define = macro.mName;
                    define.Append(TEXT("="));
                    define.Append(macro.mValue.GetData(), macro.mValue.Length());
                    args.PushBack(define);
                }
            }

            if (request.mOptions.mTargetBackend == Rhi::ERhiBackend::Vulkan) {
                AddArg(args, TEXT("-spirv"));
                AddArg(args, TEXT("-fspv-reflect"));
            }

            args.PushBack(sourcePath);
            return args;
        }

        auto GetOutputExtension(Rhi::ERhiBackend backend) -> FString {
            switch (backend) {
            case Rhi::ERhiBackend::Vulkan:
                return FString(TEXT(".spv"));
            case Rhi::ERhiBackend::DirectX11:
                return FString(TEXT(".dxbc"));
            case Rhi::ERhiBackend::DirectX12:
            default:
                return FString(TEXT(".dxil"));
            }
        }
    } // namespace

    auto FDxcCompilerBackend::GetDisplayName() const noexcept -> FStringView {
        return FStringView(kDxcName);
    }

    auto FDxcCompilerBackend::IsAvailable() const noexcept -> bool {
        return (AE_SHADER_COMPILER_ENABLE_DXC != 0) && AE_PLATFORM_WIN;
    }

    auto FDxcCompilerBackend::Compile(const FShaderCompileRequest& request)
        -> FShaderCompileResult {
        FShaderCompileResult result;
        if (!IsAvailable()) {
            result.mSucceeded   = false;
            result.mDiagnostics = FString(kDxcDisabledMessage);
            return result;
        }

        FAutoBindingOutput autoBinding;
        if (!ApplyAutoBindings(request.mSource.mPath, request.mOptions.mTargetBackend,
                autoBinding, result.mDiagnostics)) {
            result.mSucceeded = false;
            return result;
        }

        FString sourcePath = autoBinding.mApplied ? autoBinding.mSourcePath : request.mSource.mPath;

        const FString outputPath =
            BuildTempOutputPath(sourcePath, FString(TEXT("dxc")),
                GetOutputExtension(request.mOptions.mTargetBackend));
        auto args = BuildCompilerArgs(request, outputPath, sourcePath);

        FString compilerPath = request.mOptions.mCompilerPathOverride;
        if (compilerPath.IsEmptyString()) {
            compilerPath = FString(TEXT("dxc.exe"));
        }

        TVector<u32> autoSpaces;
        if (autoBinding.mApplied && request.mOptions.mTargetBackend == Rhi::ERhiBackend::Vulkan) {
            for (u32 i = 0; i < static_cast<u32>(EAutoBindingGroup::Count); ++i) {
                if (autoBinding.mLayout.mGroupUsed[i]) {
                    autoSpaces.PushBack(i);
                }
            }
        }

        if (request.mOptions.mTargetBackend == Rhi::ERhiBackend::Vulkan) {
            AppendVulkanBindingArgs(request.mOptions.mVulkanBinding,
                autoSpaces.IsEmpty() ? nullptr : &autoSpaces, args);
        }

        const auto procResult = RunProcess(compilerPath, args);
        result.mDiagnostics   = procResult.mOutput;

        if (!procResult.mSucceeded) {
            result.mSucceeded = false;
            RemoveFileIfExists(outputPath);
            return result;
        }

        if (!ReadFileBytes(outputPath, result.mBytecode)) {
            AppendDiagnosticLine(result.mDiagnostics, TEXT("Failed to read DXC output file."));
            result.mSucceeded = false;
            RemoveFileIfExists(outputPath);
            return result;
        }

        result.mOutputDebugPath = outputPath;
        result.mSucceeded       = true;

#if AE_PLATFORM_WIN
        if (request.mOptions.mTargetBackend == Rhi::ERhiBackend::Vulkan) {
            AppendDiagnosticLine(result.mDiagnostics,
                TEXT("DXC reflection for SPIR-V output is not implemented; prefer Slang for Vulkan."));
        } else if (!ExtractReflectionFromDxil(result.mBytecode, result.mReflection,
                       result.mDiagnostics)) {
            AppendDiagnosticLine(result.mDiagnostics,
                TEXT("DXC reflection extraction failed; reflection data may be incomplete."));
        }
#else
        AppendDiagnosticLine(result.mDiagnostics,
            TEXT("DXC reflection extraction not supported on this platform."));
#endif

        result.mRhiLayout = BuildRhiBindingLayout(result.mReflection, request.mSource.mStage);

        return result;
    }

} // namespace AltinaEngine::ShaderCompiler::Detail
