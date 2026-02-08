#include "RhiD3D11/RhiD3D11Device.h"
#include "RhiD3D11/RhiD3D11Pipeline.h"
#include "RhiD3D11/RhiD3D11Shader.h"

#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiCommandContext.h"
#include "Rhi/RhiCommandList.h"
#include "Rhi/RhiCommandPool.h"
#include "Rhi/RhiFence.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiSemaphore.h"
#include "Rhi/RhiShader.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"

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
    #include <Windows.h>
    #ifdef CreateSemaphore
        #undef CreateSemaphore
    #endif
    #include <d3d11.h>
    #include <wrl/client.h>
#endif

#include <string>
#include <vector>
#include <type_traits>

namespace AltinaEngine::Rhi {
    using Core::Container::TVector;
#if AE_PLATFORM_WIN
    using Microsoft::WRL::ComPtr;

    struct FRhiD3D11Device::FState {
        ComPtr<ID3D11Device>        mDevice;
        ComPtr<ID3D11DeviceContext> mImmediateContext;
        D3D_FEATURE_LEVEL           mFeatureLevel = D3D_FEATURE_LEVEL_11_0;
    };
#else
    struct FRhiD3D11Device::FState {};
#endif

    namespace {
#if AE_PLATFORM_WIN
        using Core::Container::FString;
        using Core::Container::TVector;

        auto ToD3D11Format(ERhiFormat format) noexcept -> DXGI_FORMAT {
            switch (format) {
                case ERhiFormat::R8G8B8A8_UNORM:
                    return DXGI_FORMAT_R8G8B8A8_UNORM;
                case ERhiFormat::R8G8B8A8_UNORM_SRGB:
                    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                case ERhiFormat::B8G8R8A8_UNORM:
                    return DXGI_FORMAT_B8G8R8A8_UNORM;
                case ERhiFormat::B8G8R8A8_UNORM_SRGB:
                    return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                case ERhiFormat::R16G16B16A16_FLOAT:
                    return DXGI_FORMAT_R16G16B16A16_FLOAT;
                case ERhiFormat::R32_FLOAT:
                    return DXGI_FORMAT_R32_FLOAT;
                case ERhiFormat::D24_UNORM_S8_UINT:
                    return DXGI_FORMAT_D24_UNORM_S8_UINT;
                case ERhiFormat::D32_FLOAT:
                    return DXGI_FORMAT_D32_FLOAT;
                case ERhiFormat::Unknown:
                default:
                    return DXGI_FORMAT_UNKNOWN;
            }
        }

        auto ToBindingType(const Shader::FShaderResourceBinding& resource) -> Rhi::ERhiBindingType {
            using Rhi::ERhiBindingType;
            switch (resource.mType) {
                case Shader::EShaderResourceType::ConstantBuffer:
                    return ERhiBindingType::ConstantBuffer;
                case Shader::EShaderResourceType::Texture:
                    return ERhiBindingType::SampledTexture;
                case Shader::EShaderResourceType::Sampler:
                    return ERhiBindingType::Sampler;
                case Shader::EShaderResourceType::StorageBuffer:
                    return (resource.mAccess == Shader::EShaderResourceAccess::ReadWrite)
                        ? ERhiBindingType::StorageBuffer
                        : ERhiBindingType::SampledBuffer;
                case Shader::EShaderResourceType::StorageTexture:
                    return (resource.mAccess == Shader::EShaderResourceAccess::ReadWrite)
                        ? ERhiBindingType::StorageTexture
                        : ERhiBindingType::SampledTexture;
                case Shader::EShaderResourceType::AccelerationStructure:
                    return ERhiBindingType::AccelerationStructure;
                default:
                    return ERhiBindingType::SampledTexture;
            }
        }

        auto HasLayoutBinding(const FRhiPipelineLayout* layout, u32 setIndex, u32 binding,
            Rhi::ERhiBindingType type) -> bool {
            if (layout == nullptr) {
                return true;
            }

            const auto& pipelineDesc = layout->GetDesc();
            for (const auto* group : pipelineDesc.mBindGroupLayouts) {
                if (group == nullptr) {
                    continue;
                }
                const auto& groupDesc = group->GetDesc();
                if (groupDesc.mSetIndex != setIndex) {
                    continue;
                }
                for (const auto& entry : groupDesc.mEntries) {
                    if (entry.mBinding == binding) {
                        return entry.mType == type;
                    }
                }
            }
            return false;
        }

        auto AppendReflectionBindings(const Shader::FShaderReflection& reflection, EShaderStage stage,
            const FRhiPipelineLayout* layout, TVector<FD3D11BindingMappingEntry>& outBindings) -> void {
            for (const auto& resource : reflection.mResources) {
                const auto bindingType = ToBindingType(resource);
                if (!HasLayoutBinding(layout, resource.mSet, resource.mBinding, bindingType)) {
                    continue;
                }

                FD3D11BindingMappingEntry entry{};
                entry.mStage    = stage;
                entry.mType     = bindingType;
                entry.mSet      = resource.mSet;
                entry.mBinding  = resource.mBinding;
                entry.mRegister = resource.mRegister;
                entry.mSpace    = resource.mSpace;
                outBindings.PushBack(entry);
            }
        }

        auto ToAnsiString(const FString& text) -> std::string {
            std::string out;
            const auto  length = static_cast<size_t>(text.Length());
            out.reserve(length);
            const auto* data = text.GetData();
            for (size_t i = 0; i < length; ++i) {
                const auto ch = data[i];
                out.push_back((ch <= 0x7f) ? static_cast<char>(ch) : '?');
            }
            return out;
        }

        auto BuildInputLayout(const FRhiGraphicsPipelineDesc& desc, ID3D11Device* device)
            -> ComPtr<ID3D11InputLayout> {
            ComPtr<ID3D11InputLayout> layout;
            if (device == nullptr || desc.mVertexShader == nullptr) {
                return layout;
            }

            const auto& shaderDesc = desc.mVertexShader->GetDesc();
            if (shaderDesc.mBytecode.IsEmpty() || desc.mVertexLayout.mAttributes.IsEmpty()) {
                return layout;
            }

            TVector<D3D11_INPUT_ELEMENT_DESC> elements;
            elements.Reserve(desc.mVertexLayout.mAttributes.Size());

            std::vector<std::string> semanticStorage;
            semanticStorage.reserve(
                static_cast<size_t>(desc.mVertexLayout.mAttributes.Size()));

            for (const auto& attribute : desc.mVertexLayout.mAttributes) {
                const DXGI_FORMAT format = ToD3D11Format(attribute.mFormat);
                if (format == DXGI_FORMAT_UNKNOWN) {
                    return layout;
                }

                semanticStorage.push_back(ToAnsiString(attribute.mSemanticName));

                D3D11_INPUT_ELEMENT_DESC element{};
                element.SemanticName         = semanticStorage.back().c_str();
                element.SemanticIndex        = attribute.mSemanticIndex;
                element.Format               = format;
                element.InputSlot            = attribute.mInputSlot;
                element.AlignedByteOffset    = attribute.mAlignedByteOffset;
                element.InputSlotClass       =
                    attribute.mPerInstance ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
                element.InstanceDataStepRate =
                    attribute.mPerInstance ? (attribute.mInstanceStepRate == 0U
                                                  ? 1U
                                                  : attribute.mInstanceStepRate)
                                            : 0U;
                elements.PushBack(element);
            }

            const void* data  = shaderDesc.mBytecode.Data();
            const SIZE_T size = static_cast<SIZE_T>(shaderDesc.mBytecode.Size());
            if (data == nullptr || size == 0U) {
                return layout;
            }

            const HRESULT hr = device->CreateInputLayout(elements.Data(),
                static_cast<UINT>(elements.Size()), data, size, &layout);
            if (FAILED(hr)) {
                layout.Reset();
            }
            return layout;
        }
#endif

        class FRhiD3D11PipelineLayout final : public FRhiPipelineLayout {
        public:
            explicit FRhiD3D11PipelineLayout(const FRhiPipelineLayoutDesc& desc)
                : FRhiPipelineLayout(desc) {}
        };

        class FRhiD3D11BindGroupLayout final : public FRhiBindGroupLayout {
        public:
            explicit FRhiD3D11BindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
                : FRhiBindGroupLayout(desc) {}
        };

        class FRhiD3D11BindGroup final : public FRhiBindGroup {
        public:
            explicit FRhiD3D11BindGroup(const FRhiBindGroupDesc& desc)
                : FRhiBindGroup(desc) {}
        };

        class FRhiD3D11Fence final : public FRhiFence {
        public:
            explicit FRhiD3D11Fence(u64 initialValue)
                : FRhiFence(), mValue(initialValue) {}

            [[nodiscard]] auto GetCompletedValue() const noexcept -> u64 override {
                return mValue;
            }
            void SignalCPU(u64 value) override { mValue = value; }
            void WaitCPU(u64 value) override { mValue = value; }
            void Reset(u64 value) override { mValue = value; }

        private:
            u64 mValue = 0ULL;
        };

        class FRhiD3D11Semaphore final : public FRhiSemaphore {
        public:
            FRhiD3D11Semaphore(bool timeline, u64 initialValue)
                : FRhiSemaphore(), mIsTimeline(timeline), mValue(initialValue) {}

            [[nodiscard]] auto IsTimeline() const noexcept -> bool override {
                return mIsTimeline;
            }
            [[nodiscard]] auto GetCurrentValue() const noexcept -> u64 override {
                return mValue;
            }

        private:
            bool mIsTimeline = false;
            u64  mValue      = 0ULL;
        };

        class FRhiD3D11CommandPool final : public FRhiCommandPool {
        public:
            explicit FRhiD3D11CommandPool(const FRhiCommandPoolDesc& desc)
                : FRhiCommandPool(desc) {}

            void Reset() override {}
        };

        class FRhiD3D11CommandList final : public FRhiCommandList {
        public:
            explicit FRhiD3D11CommandList(const FRhiCommandListDesc& desc)
                : FRhiCommandList(desc) {}

            void Reset(FRhiCommandPool* /*pool*/) override {}
            void Close() override {}
        };

        class FRhiD3D11CommandContext final : public FRhiCommandContext {
        public:
            FRhiD3D11CommandContext(const FRhiCommandContextDesc& desc,
                FRhiCommandListRef commandList)
                : FRhiCommandContext(desc), mCommandList(AltinaEngine::Move(commandList)) {}

            void Begin() override {}
            void End() override {}
            [[nodiscard]] auto GetCommandList() const noexcept -> FRhiCommandList* override {
                return mCommandList.Get();
            }

        private:
            FRhiCommandListRef mCommandList;
        };

        class FRhiD3D11Queue final : public FRhiQueue {
        public:
            explicit FRhiD3D11Queue(ERhiQueueType type) : FRhiQueue(type) {}

            void Submit(const FRhiSubmitInfo& /*info*/) override {}
            void Signal(FRhiFence* /*fence*/, u64 /*value*/) override {}
            void Wait(FRhiFence* /*fence*/, u64 /*value*/) override {}
            void WaitIdle() override {}
            void Present(const FRhiPresentInfo& /*info*/) override {}
        };
    } // namespace

#if AE_PLATFORM_WIN
    struct FRhiD3D11GraphicsPipeline::FState {
        ComPtr<ID3D11InputLayout> mInputLayout;
    };
#else
    struct FRhiD3D11GraphicsPipeline::FState {};
#endif

    FRhiD3D11GraphicsPipeline::FRhiD3D11GraphicsPipeline(
        const FRhiGraphicsPipelineDesc& desc, ID3D11Device* device)
        : FRhiPipeline(desc)
        , mPipelineLayout(desc.mPipelineLayout)
        , mVertexShader(desc.mVertexShader)
        , mPixelShader(desc.mPixelShader)
        , mGeometryShader(desc.mGeometryShader)
        , mHullShader(desc.mHullShader)
        , mDomainShader(desc.mDomainShader) {
#if AE_PLATFORM_WIN
        mState = new FState{};
        if (mState) {
            mState->mInputLayout = BuildInputLayout(desc, device);
        }
        const FRhiPipelineLayout* layout = desc.mPipelineLayout;
        if (desc.mVertexShader) {
            AppendReflectionBindings(desc.mVertexShader->GetDesc().mReflection,
                EShaderStage::Vertex, layout, mBindings);
        }
        if (desc.mPixelShader) {
            AppendReflectionBindings(desc.mPixelShader->GetDesc().mReflection,
                EShaderStage::Pixel, layout, mBindings);
        }
        if (desc.mGeometryShader) {
            AppendReflectionBindings(desc.mGeometryShader->GetDesc().mReflection,
                EShaderStage::Geometry, layout, mBindings);
        }
        if (desc.mHullShader) {
            AppendReflectionBindings(desc.mHullShader->GetDesc().mReflection,
                EShaderStage::Hull, layout, mBindings);
        }
        if (desc.mDomainShader) {
            AppendReflectionBindings(desc.mDomainShader->GetDesc().mReflection,
                EShaderStage::Domain, layout, mBindings);
        }
#else
        (void)desc;
        (void)device;
#endif
    }

    FRhiD3D11GraphicsPipeline::~FRhiD3D11GraphicsPipeline() {
#if AE_PLATFORM_WIN
        delete mState;
        mState = nullptr;
#endif
    }

    auto FRhiD3D11GraphicsPipeline::GetInputLayout() const noexcept -> ID3D11InputLayout* {
#if AE_PLATFORM_WIN
        return mState ? mState->mInputLayout.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11GraphicsPipeline::GetBindingMappings() const noexcept
        -> const TVector<FD3D11BindingMappingEntry>& {
        return mBindings;
    }

    FRhiD3D11ComputePipeline::FRhiD3D11ComputePipeline(const FRhiComputePipelineDesc& desc)
        : FRhiPipeline(desc)
        , mPipelineLayout(desc.mPipelineLayout)
        , mComputeShader(desc.mComputeShader) {
#if AE_PLATFORM_WIN
        if (desc.mComputeShader) {
            AppendReflectionBindings(desc.mComputeShader->GetDesc().mReflection,
                EShaderStage::Compute, desc.mPipelineLayout, mBindings);
        }
#else
        (void)desc;
#endif
    }

    FRhiD3D11ComputePipeline::~FRhiD3D11ComputePipeline() = default;

    auto FRhiD3D11ComputePipeline::GetBindingMappings() const noexcept
        -> const TVector<FD3D11BindingMappingEntry>& {
        return mBindings;
    }

    FRhiD3D11Device::FRhiD3D11Device(const FRhiDeviceDesc& desc,
        const FRhiAdapterDesc& adapterDesc, ID3D11Device* device,
        ID3D11DeviceContext* context, u32 featureLevel)
        : FRhiDevice(desc, adapterDesc) {
        mState = new FState{};

#if AE_PLATFORM_WIN
        if (mState) {
            mState->mDevice.Attach(device);
            mState->mImmediateContext.Attach(context);
            mState->mFeatureLevel = static_cast<D3D_FEATURE_LEVEL>(featureLevel);
        }

        FRhiSupportedLimits limits;
        limits.mMaxTextureDimension1D     = D3D11_REQ_TEXTURE1D_U_DIMENSION;
        limits.mMaxTextureDimension2D     = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        limits.mMaxTextureDimension3D     = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
        limits.mMaxTextureArrayLayers     = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
        limits.mMaxSamplers               = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
        limits.mMaxColorAttachments       = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
        SetSupportedLimits(limits);

        FRhiQueueCapabilities queueCaps;
        queueCaps.mSupportsGraphics = true;
        queueCaps.mSupportsCompute  = true;
        queueCaps.mSupportsCopy     = true;
        queueCaps.mSupportsAsyncCompute = false;
        queueCaps.mSupportsAsyncCopy    = false;
        SetQueueCapabilities(queueCaps);
#else
        (void)device;
        (void)context;
        (void)featureLevel;
#endif

        RegisterQueue(ERhiQueueType::Graphics,
            MakeResource<FRhiD3D11Queue>(ERhiQueueType::Graphics));
        RegisterQueue(ERhiQueueType::Compute,
            MakeResource<FRhiD3D11Queue>(ERhiQueueType::Compute));
        RegisterQueue(ERhiQueueType::Copy,
            MakeResource<FRhiD3D11Queue>(ERhiQueueType::Copy));
    }

    FRhiD3D11Device::~FRhiD3D11Device() {
        delete mState;
        mState = nullptr;
    }

    auto FRhiD3D11Device::GetNativeDevice() const noexcept -> ID3D11Device* {
#if AE_PLATFORM_WIN
        return mState ? mState->mDevice.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Device::GetImmediateContext() const noexcept -> ID3D11DeviceContext* {
#if AE_PLATFORM_WIN
        return mState ? mState->mImmediateContext.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Device::GetFeatureLevel() const noexcept -> u32 {
#if AE_PLATFORM_WIN
        return mState ? static_cast<u32>(mState->mFeatureLevel) : 0U;
#else
        return 0U;
#endif
    }

    auto FRhiD3D11Device::CreateShader(const FRhiShaderDesc& desc) -> FRhiShaderRef {
#if AE_PLATFORM_WIN
        ID3D11Device* device = GetNativeDevice();
        if (!device || desc.mBytecode.IsEmpty()) {
            return {};
        }

        const void* data = desc.mBytecode.Data();
        const SIZE_T size = static_cast<SIZE_T>(desc.mBytecode.Size());
        if (!data || size == 0U) {
            return {};
        }

        HRESULT hr = E_FAIL;
        ComPtr<ID3D11DeviceChild> shader;

        switch (desc.mStage) {
        case EShaderStage::Vertex: {
            ComPtr<ID3D11VertexShader> vertexShader;
            hr = device->CreateVertexShader(data, size, nullptr, &vertexShader);
            shader = vertexShader;
            break;
        }
        case EShaderStage::Pixel: {
            ComPtr<ID3D11PixelShader> pixelShader;
            hr = device->CreatePixelShader(data, size, nullptr, &pixelShader);
            shader = pixelShader;
            break;
        }
        case EShaderStage::Compute: {
            ComPtr<ID3D11ComputeShader> computeShader;
            hr = device->CreateComputeShader(data, size, nullptr, &computeShader);
            shader = computeShader;
            break;
        }
        case EShaderStage::Geometry: {
            ComPtr<ID3D11GeometryShader> geometryShader;
            hr = device->CreateGeometryShader(data, size, nullptr, &geometryShader);
            shader = geometryShader;
            break;
        }
        case EShaderStage::Hull: {
            ComPtr<ID3D11HullShader> hullShader;
            hr = device->CreateHullShader(data, size, nullptr, &hullShader);
            shader = hullShader;
            break;
        }
        case EShaderStage::Domain: {
            ComPtr<ID3D11DomainShader> domainShader;
            hr = device->CreateDomainShader(data, size, nullptr, &domainShader);
            shader = domainShader;
            break;
        }
        case EShaderStage::Mesh:
        case EShaderStage::Amplification:
        case EShaderStage::Library:
        default:
            return {};
        }

        if (FAILED(hr) || shader == nullptr) {
            return {};
        }

        return MakeResource<FRhiD3D11Shader>(desc, shader.Detach());
#else
        return MakeResource<FRhiD3D11Shader>(desc);
#endif
    }

    auto FRhiD3D11Device::CreateGraphicsPipeline(const FRhiGraphicsPipelineDesc& desc)
        -> FRhiPipelineRef {
#if AE_PLATFORM_WIN
        return MakeResource<FRhiD3D11GraphicsPipeline>(desc, GetNativeDevice());
#else
        return MakeResource<FRhiD3D11GraphicsPipeline>(desc);
#endif
    }

    auto FRhiD3D11Device::CreateComputePipeline(const FRhiComputePipelineDesc& desc)
        -> FRhiPipelineRef {
        return MakeResource<FRhiD3D11ComputePipeline>(desc);
    }

    auto FRhiD3D11Device::CreatePipelineLayout(const FRhiPipelineLayoutDesc& desc)
        -> FRhiPipelineLayoutRef {
        return MakeResource<FRhiD3D11PipelineLayout>(desc);
    }

    auto FRhiD3D11Device::CreateBindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
        -> FRhiBindGroupLayoutRef {
        return MakeResource<FRhiD3D11BindGroupLayout>(desc);
    }

    auto FRhiD3D11Device::CreateBindGroup(const FRhiBindGroupDesc& desc)
        -> FRhiBindGroupRef {
        return MakeResource<FRhiD3D11BindGroup>(desc);
    }

    auto FRhiD3D11Device::CreateFence(u64 initialValue) -> FRhiFenceRef {
        return MakeResource<FRhiD3D11Fence>(initialValue);
    }

    auto FRhiD3D11Device::CreateSemaphore(bool timeline, u64 initialValue) -> FRhiSemaphoreRef {
        return MakeResource<FRhiD3D11Semaphore>(timeline, initialValue);
    }

    auto FRhiD3D11Device::CreateCommandPool(const FRhiCommandPoolDesc& desc)
        -> FRhiCommandPoolRef {
        return MakeResource<FRhiD3D11CommandPool>(desc);
    }

    auto FRhiD3D11Device::CreateCommandList(const FRhiCommandListDesc& desc)
        -> FRhiCommandListRef {
        return MakeResource<FRhiD3D11CommandList>(desc);
    }

    auto FRhiD3D11Device::CreateCommandContext(const FRhiCommandContextDesc& desc)
        -> FRhiCommandContextRef {
        FRhiCommandListDesc listDesc;
        listDesc.mDebugName = desc.mDebugName;
        listDesc.mQueueType = desc.mQueueType;
        listDesc.mListType  = desc.mListType;
        auto commandList = MakeResource<FRhiD3D11CommandList>(listDesc);
        return MakeResource<FRhiD3D11CommandContext>(desc, AltinaEngine::Move(commandList));
    }

} // namespace AltinaEngine::Rhi
