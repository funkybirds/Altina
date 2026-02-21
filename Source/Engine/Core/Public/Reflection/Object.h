#pragma once
#include "Types/Meta.h"
#include "ReflectionBase.h"
#include "Utility/CompilerHint.h"
#include "ReflectionFwd.h"
namespace AltinaEngine::Core::Reflection {
    using namespace TypeMeta;

    class ISerializer;
    class IDeserializer;

    class FObject {
    public:
        FObject() noexcept : mPtr(nullptr), mMetadata(FMetaTypeInfo::CreatePlaceHolder()) {}
        FObject(FObject&& rhs) noexcept : mPtr(rhs.mPtr), mMetadata(rhs.mMetadata) {
            rhs.mPtr      = nullptr;
            rhs.mMetadata = FMetaTypeInfo::CreatePlaceHolder();
        }
        FObject(const FObject& rhs) : mPtr(nullptr), mMetadata(rhs.mMetadata) {
            ConstructFromMetadataCopyCtor(rhs);
        }
        ~FObject() { DestructFromMetadata(); }

        auto operator=(FObject&& rhs) noexcept -> FObject& {
            if (this != &rhs) [[likely]] {
                if (mPtr != nullptr) [[likely]] {
                    this->~FObject();
                }
                mPtr          = rhs.mPtr;
                mMetadata     = rhs.mMetadata;
                rhs.mPtr      = nullptr;
                rhs.mMetadata = FMetaTypeInfo::CreatePlaceHolder();
            }
            return *this;
        }
        auto operator=(const FObject& rhs) noexcept -> FObject& {
            if (this != &rhs) [[likely]] {
                if (mPtr != nullptr) [[likely]] {
                    DestructFromMetadata();
                }
                // Ensure metadata matches the source before invoking copy constructor
                mMetadata = rhs.mMetadata;
                ConstructFromMetadataCopyCtor(rhs);
            }
            return *this;
        }

        // Copy Constructors
        template <CCopyConstructible T> auto operator=(T& rhs) -> FObject& {
            if (mPtr != &rhs) [[likely]] {
                if (mPtr) [[likely]] {
                    DestructFromMetadata();
                }
                ConstructFromCopyCtor<T>(rhs);
            }
            return *this;
        }

        // Casting
        template <CDecayed T> auto As() -> T& {
            FReflectionDumpData nullDump;
            nullDump.mObjectPtr      = mPtr;
            nullDump.mTypeInfo       = &mMetadata;
            nullDump.mTypeHash       = mMetadata.GetHash();
            nullDump.mObjectTypeHash = mMetadata.GetHash();
            ReflectionAssert(mPtr != nullptr, EReflectionErrorCode::DereferenceNullptr, nullDump);
            if (mMetadata.GetTypeInfo() == typeid(T) && mPtr) [[likely]] {
                return *static_cast<T*>(mPtr);
            }
            if (auto casted = Detail::TryChainedUpcast(
                    mPtr, mMetadata.GetHash(), FMetaTypeInfo::Create<T>().GetHash())) {
                return *static_cast<T*>(casted);
            }
            FMetaTypeInfo       expected = FMetaTypeInfo::Create<T>();
            FReflectionDumpData castDump;
            castDump.mObjectPtr        = mPtr;
            castDump.mTypeInfo         = &mMetadata;
            castDump.mExpectedTypeInfo = &expected;
            castDump.mTypeHash         = mMetadata.GetHash();
            castDump.mExpectedTypeHash = expected.GetHash();
            castDump.mObjectTypeHash   = mMetadata.GetHash();
            ReflectionAssert(false, EReflectionErrorCode::CorruptedAnyCast, castDump);
            Utility::CompilerHint::Unreachable();
        }
        template <CDecayed T> auto As() const -> const T& {
            FReflectionDumpData nullDump;
            nullDump.mObjectPtr      = mPtr;
            nullDump.mTypeInfo       = &mMetadata;
            nullDump.mTypeHash       = mMetadata.GetHash();
            nullDump.mObjectTypeHash = mMetadata.GetHash();
            ReflectionAssert(mPtr != nullptr, EReflectionErrorCode::DereferenceNullptr, nullDump);
            if (mMetadata.GetTypeInfo() == typeid(T) && mPtr) [[likely]] {
                return *static_cast<const T*>(mPtr);
            }
            if (auto casted = Detail::TryChainedUpcast(
                    mPtr, mMetadata.GetHash(), FMetaTypeInfo::Create<T>().GetHash())) {
                return *static_cast<const T*>(casted);
            }
            FMetaTypeInfo       expected = FMetaTypeInfo::Create<T>();
            FReflectionDumpData castDump;
            castDump.mObjectPtr        = mPtr;
            castDump.mTypeInfo         = &mMetadata;
            castDump.mExpectedTypeInfo = &expected;
            castDump.mTypeHash         = mMetadata.GetHash();
            castDump.mExpectedTypeHash = expected.GetHash();
            castDump.mObjectTypeHash   = mMetadata.GetHash();
            ReflectionAssert(false, EReflectionErrorCode::CorruptedAnyCast, castDump);
            Utility::CompilerHint::Unreachable();
        }
        // Metadata
        [[nodiscard]] auto GetTypeHash() const noexcept -> FTypeMetaHash {
            return mMetadata.GetHash();
        }
        [[nodiscard]] auto GetTypeInfo() const noexcept -> FTypeInfo const& {
            return mMetadata.GetTypeInfo();
        }

        // Serialization
        void Serialize(ISerializer& serializer) const;
        void Deserialize(IDeserializer& deserializer);

        // Constructors
        template <CNonVoid T, typename... TArgs> static auto Create(TArgs&&... args) -> FObject {
            T*            pObject  = new T(Forward<TArgs>(args)...);
            FMetaTypeInfo metadata = FMetaTypeInfo::Create<T>();
            return FObject(pObject, metadata);
        }
        template <CVoid> static auto Create() {
            FMetaTypeInfo metadata = FMetaTypeInfo::CreateVoid();
            return FObject(nullptr, metadata);
        }
        template <CCopyConstructible T, typename... TArgs>
        static auto CreateClone(const T& value) -> FObject {
            T*            pObject  = new T(value);
            FMetaTypeInfo metadata = FMetaTypeInfo::Create<T>();
            return FObject(pObject, metadata);
        }
        static auto CreateFromMetadata(void* ptr, const FMetaTypeInfo& meta) {
            return FObject(ptr, meta);
        }

    private:
        // Private Constructor
        FObject(void* ptr, const FMetaTypeInfo& metadata) noexcept
            : mPtr(ptr), mMetadata(metadata) {}

        template <CCopyConstructible T> void ConstructFromCopyCtor(const T& rhs) {
            mPtr      = new T(rhs);
            mMetadata = FMetaTypeInfo::Create<T>();
        }
        void ConstructFromMetadataCopyCtor(const FObject& rhs) {
            FReflectionDumpData dump;
            dump.mObjectPtr      = rhs.mPtr;
            dump.mTypeInfo       = &mMetadata;
            dump.mTypeHash       = mMetadata.GetHash();
            dump.mObjectTypeHash = mMetadata.GetHash();
            if (ReflectionAssert(mMetadata.IsCopyConstructible(),
                    EReflectionErrorCode::TypeNotCopyConstructible, dump)) [[likely]] {
                mPtr = mMetadata.CallCopyConstructor(rhs.mPtr);
            }
        }
        void DestructFromMetadata() const {
            if (mPtr == nullptr)
                return;
            FReflectionDumpData dump;
            dump.mObjectPtr      = mPtr;
            dump.mTypeInfo       = &mMetadata;
            dump.mTypeHash       = mMetadata.GetHash();
            dump.mObjectTypeHash = mMetadata.GetHash();
            if (ReflectionAssert(mMetadata.IsDestructible(),
                    EReflectionErrorCode::TypeNotDestructible, dump)) [[likely]] {
                mMetadata.CallDestructor(mPtr);
            }
        }

        void*         mPtr;
        FMetaTypeInfo mMetadata;
    };
} // namespace AltinaEngine::Core::Reflection
