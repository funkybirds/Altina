#pragma once
#include "Types/Meta.h"
#include "ReflectionBase.h"
#include "Utility/CompilerHint.h"
namespace AltinaEngine::Core::Reflection
{
    using namespace TypeMeta;

    class FObject
    {
    public:
        FObject() noexcept : mPtr(nullptr), mMetadata(FMetaTypeInfo::CreatePlaceHolder()) {}
        FObject(FObject&& rhs) noexcept : mPtr(rhs.mPtr), mMetadata(rhs.mMetadata)
        {
            rhs.mPtr      = nullptr;
            rhs.mMetadata = FMetaTypeInfo::CreatePlaceHolder();
        }
        FObject(const FObject& rhs) : mMetadata(rhs.mMetadata) { ConstructFromMetadataCopyCtor(rhs); }
        ~FObject() { DestructFromMetadata(); }

        auto operator=(FObject&& rhs) noexcept -> FObject&
        {
            if (this != &rhs) [[likely]]
            {
                if (mPtr) [[likely]]
                {
                    this->~FObject();
                }
                mPtr          = rhs.mPtr;
                mMetadata     = rhs.mMetadata;
                rhs.mPtr      = nullptr;
                rhs.mMetadata = FMetaTypeInfo::CreatePlaceHolder();
            }
            return *this;
        }
        auto operator=(const FObject& rhs) noexcept -> FObject&
        {
            if (this != &rhs) [[likely]]
            {
                if (mPtr) [[likely]]
                {
                    DestructFromMetadata();
                }
                // Ensure metadata matches the source before invoking copy constructor
                mMetadata = rhs.mMetadata;
                ConstructFromMetadataCopyCtor(rhs);
            }
            return *this;
        }

        // Copy Constructors
        template <ICopyConstructible T> auto operator=(T& rhs) -> FObject&
        {
            if (mPtr != &rhs) [[likely]]
            {
                if (mPtr) [[likely]]
                {
                    DestructFromMetadata();
                }
                ConstructFromCopyCtor<T>(rhs);
            }
            return *this;
        }

        // Casting
        template <typename T> auto As() -> T&
        {
            if (ReflectionAssert(mMetadata.GetTypeInfo() == typeid(T), EReflectionErrorCode::CorruptedAnyCast,
                    FReflectionDumpData{})) [[likely]]
            {
                return *static_cast<T*>(mPtr);
            }
            Utility::CompilerHint::Unreachable();
        }
        template <typename T> auto As() const -> T&
        {
            if (ReflectionAssert(mMetadata.GetTypeInfo() == typeid(T), EReflectionErrorCode::CorruptedAnyCast,
                    FReflectionDumpData{})) [[likely]]
            {
                return *static_cast<T*>(mPtr);
            }
            Utility::CompilerHint::Unreachable();
        }

        // Constructors
        template <INonVoid T, typename... TArgs> static auto Create(TArgs&&... args) -> FObject
        {
            T*            pObject  = new T(Forward<TArgs>(args)...);
            FMetaTypeInfo metadata = FMetaTypeInfo::Create<T>();
            return FObject(pObject, metadata);
        }
        template <IVoid> static auto Create()
        {
            FMetaTypeInfo metadata = FMetaTypeInfo::CreateVoid();
            return FObject(nullptr, metadata);
        }
        template <ICopyConstructible T, typename... TArgs> static auto CreateClone(const T& value) -> FObject
        {
            T*            pObject  = new T(value);
            FMetaTypeInfo metadata = FMetaTypeInfo::Create<T>();
            return FObject(pObject, metadata);
        }

    private:
        // Private Constructor
        FObject(void* ptr, const FMetaTypeInfo& metadata) noexcept : mPtr(ptr), mMetadata(metadata) {}

        template <ICopyConstructible T> void ConstructFromCopyCtor(const T& rhs)
        {
            mPtr      = new T(rhs);
            mMetadata = FMetaTypeInfo::Create<T>();
        }
        void ConstructFromMetadataCopyCtor(const FObject& rhs)
        {
            if (ReflectionAssert(mMetadata.IsCopyConstructible(), EReflectionErrorCode::TypeNotCopyConstructible,
                    FReflectionDumpData{})) [[likely]]
            {
                mPtr = mMetadata.CallCopyConstructor(rhs.mPtr);
            }
        }
        void DestructFromMetadata() const
        {
            if (!mPtr)
                return;
            if (ReflectionAssert(mMetadata.IsDestructible(), EReflectionErrorCode::TypeNotDestructible,
                    FReflectionDumpData{})) [[likely]]
            {
                mMetadata.CallDestructor(mPtr);
            }
        }

        void*         mPtr;
        FMetaTypeInfo mMetadata;
    };
} // namespace AltinaEngine::Core::Reflection