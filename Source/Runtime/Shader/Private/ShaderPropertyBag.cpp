#include "Shader/ShaderPropertyBag.h"

#include <cstring>

namespace AltinaEngine::Shader {
    auto FShaderPropertyBag::FStringHash::operator()(const FString& s) const noexcept -> usize {
        usize h    = 5381;
        auto  view = s.ToView();
        for (usize i = 0; i < view.Length(); ++i) {
            h = ((h << 5) + h) + static_cast<unsigned char>(view.Data()[i]);
        }
        return h;
    }

    auto FShaderPropertyBag::FStringEqual::operator()(
        const FString& a, const FString& b) const noexcept -> bool {
        if (a.Length() != b.Length()) {
            return false;
        }
        auto va = a.ToView();
        auto vb = b.ToView();
        for (usize i = 0; i < va.Length(); ++i) {
            if (va.Data()[i] != vb.Data()[i]) {
                return false;
            }
        }
        return true;
    }

    void FShaderPropertyBag::Init(const FShaderConstantBuffer& cbuffer) {
        mName      = cbuffer.mName;
        mSizeBytes = cbuffer.mSizeBytes;
        mSet       = cbuffer.mSet;
        mBinding   = cbuffer.mBinding;
        mRegister  = cbuffer.mRegister;
        mSpace     = cbuffer.mSpace;

        mProperties.clear();
        mProperties.reserve(static_cast<size_t>(cbuffer.mMembers.Size()));
        for (const auto& member : cbuffer.mMembers) {
            FPropertyDesc desc{};
            desc.mOffset              = member.mOffset;
            desc.mSize                = member.mSize;
            desc.mElementCount        = member.mElementCount;
            desc.mElementStride       = member.mElementStride;
            mProperties[member.mName] = desc;
        }

        mData.Resize(mSizeBytes);
        if (!mData.IsEmpty()) {
            std::memset(mData.Data(), 0, static_cast<size_t>(mData.Size()));
        }
    }

    void FShaderPropertyBag::Reset() {
        mName.Clear();
        mSizeBytes = 0U;
        mSet       = 0U;
        mBinding   = 0U;
        mRegister  = 0U;
        mSpace     = 0U;
        mData.Clear();
        mProperties.clear();
    }

    auto FShaderPropertyBag::FindProperty(const FString& name) const noexcept
        -> const FPropertyDesc* {
        const auto it = mProperties.find(name);
        if (it == mProperties.end()) {
            return nullptr;
        }
        return &it->second;
    }

    auto FShaderPropertyBag::SetRaw(const FString& name, const void* data, u32 sizeBytes) -> bool {
        if (data == nullptr || sizeBytes == 0U) {
            return false;
        }
        const auto* desc = FindProperty(name);
        if (desc == nullptr || desc->mSize == 0U) {
            return false;
        }
        if (sizeBytes > desc->mSize) {
            return false;
        }
        const u64 endOffset = static_cast<u64>(desc->mOffset) + sizeBytes;
        if (endOffset > static_cast<u64>(mData.Size())) {
            return false;
        }

        std::memcpy(mData.Data() + desc->mOffset, data, sizeBytes);
        return true;
    }

    auto FShaderPropertyBag::SetRaw(const TChar* name, const void* data, u32 sizeBytes) -> bool {
        if (name == nullptr) {
            return false;
        }
        return SetRaw(FString(name), data, sizeBytes);
    }
} // namespace AltinaEngine::Shader
