#include "Reflection/ReflectionBase.h"
#include "Logging/Log.h"
#include <cstdint>
#include <string>
namespace AltinaEngine::Core::Reflection {
    namespace {
        using Container::FNativeStringView;

        auto ToLogString(FNativeStringView view) -> std::basic_string<TChar> {
            std::basic_string<TChar> out;
            out.reserve(view.Length());
            for (usize i = 0; i < view.Length(); ++i) {
                out.push_back(static_cast<TChar>(view[i]));
            }
            return out;
        }

        auto FormatTypeName(const FMetaTypeInfo* info) -> std::basic_string<TChar> {
            if (info == nullptr) {
                return std::basic_string<TChar>(TEXT("<null>"));
            }
            const auto name = info->GetName();
            if (name.IsEmpty()) {
                return std::basic_string<TChar>(TEXT("<unnamed>"));
            }
            return ToLogString(name);
        }

        auto FormatPropertyName(const FMetaPropertyInfo* info) -> std::basic_string<TChar> {
            if (info == nullptr) {
                return std::basic_string<TChar>(TEXT("<null>"));
            }
            const auto name = info->GetName();
            if (name.IsEmpty()) {
                return std::basic_string<TChar>(TEXT("<unnamed>"));
            }
            return ToLogString(name);
        }

        auto FormatMethodName(const FMetaMethodInfo* info) -> std::basic_string<TChar> {
            if (info == nullptr) {
                return std::basic_string<TChar>(TEXT("<null>"));
            }
            const auto name = info->GetName();
            if (name.IsEmpty()) {
                return std::basic_string<TChar>(TEXT("<unnamed>"));
            }
            return ToLogString(name);
        }

        void LogDumpData(const FReflectionDumpData& dumpData) {
            if (dumpData.mObjectPtr != nullptr) {
                LogErrorCat(TEXT("Core.Reflection"), TEXT("  ObjectPtr: {:#x}"),
                    reinterpret_cast<std::uintptr_t>(dumpData.mObjectPtr));
            }

            if (dumpData.mTypeInfo != nullptr) {
                LogErrorCat(TEXT("Core.Reflection"), TEXT("  Type: {} (hash={:#x})"),
                    FormatTypeName(dumpData.mTypeInfo),
                    static_cast<u64>(dumpData.mTypeInfo->GetHash()));
            } else if (dumpData.mTypeHash != 0) {
                LogErrorCat(TEXT("Core.Reflection"), TEXT("  TypeHash: {:#x}"),
                    static_cast<u64>(dumpData.mTypeHash));
            }

            if (dumpData.mExpectedTypeInfo != nullptr) {
                LogErrorCat(TEXT("Core.Reflection"), TEXT("  ExpectedType: {} (hash={:#x})"),
                    FormatTypeName(dumpData.mExpectedTypeInfo),
                    static_cast<u64>(dumpData.mExpectedTypeInfo->GetHash()));
            } else if (dumpData.mExpectedTypeHash != 0) {
                LogErrorCat(TEXT("Core.Reflection"), TEXT("  ExpectedTypeHash: {:#x}"),
                    static_cast<u64>(dumpData.mExpectedTypeHash));
            }

            if (dumpData.mObjectTypeHash != 0) {
                LogErrorCat(TEXT("Core.Reflection"), TEXT("  ObjectTypeHash: {:#x}"),
                    static_cast<u64>(dumpData.mObjectTypeHash));
            }

            if (dumpData.mPropertyInfo != nullptr) {
                LogErrorCat(TEXT("Core.Reflection"), TEXT("  Property: {} (hash={:#x})"),
                    FormatPropertyName(dumpData.mPropertyInfo),
                    static_cast<u64>(dumpData.mPropertyInfo->GetHash()));
                LogErrorCat(TEXT("Core.Reflection"), TEXT("  PropertyOwner: {} (hash={:#x})"),
                    FormatTypeName(&dumpData.mPropertyInfo->GetClassTypeMetadata()),
                    static_cast<u64>(dumpData.mPropertyInfo->GetClassTypeMetadata().GetHash()));
            } else if (dumpData.mPropertyHash != 0) {
                LogErrorCat(TEXT("Core.Reflection"), TEXT("  PropertyHash: {:#x}"),
                    static_cast<u64>(dumpData.mPropertyHash));
            }

            if (dumpData.mMethodInfo != nullptr) {
                LogErrorCat(TEXT("Core.Reflection"), TEXT("  Method: {} (hash={:#x})"),
                    FormatMethodName(dumpData.mMethodInfo),
                    static_cast<u64>(dumpData.mMethodInfo->GetHash()));
                LogErrorCat(TEXT("Core.Reflection"), TEXT("  MethodOwner: {} (hash={:#x})"),
                    FormatTypeName(&dumpData.mMethodInfo->GetClassTypeMetadata()),
                    static_cast<u64>(dumpData.mMethodInfo->GetClassTypeMetadata().GetHash()));
            } else if (dumpData.mMethodHash != 0) {
                LogErrorCat(TEXT("Core.Reflection"), TEXT("  MethodHash: {:#x}"),
                    static_cast<u64>(dumpData.mMethodHash));
            }

            if ((dumpData.mExpectedArgumentCount != 0) || (dumpData.mArgumentCount != 0)) {
                LogErrorCat(TEXT("Core.Reflection"), TEXT("  Args: {} / {}"),
                    static_cast<u64>(dumpData.mArgumentCount),
                    static_cast<u64>(dumpData.mExpectedArgumentCount));
            }

            if (dumpData.mReadTypeHash != 0) {
                LogErrorCat(TEXT("Core.Reflection"), TEXT("  ReadTypeHash: {:#x}"),
                    static_cast<u64>(dumpData.mReadTypeHash));
            }

            if (dumpData.mArchiveSize != 0) {
                LogErrorCat(TEXT("Core.Reflection"), TEXT("  Archive: offset={} size={}"),
                    static_cast<u64>(dumpData.mArchiveOffset),
                    static_cast<u64>(dumpData.mArchiveSize));
            }
        }
    } // namespace

    [[noreturn]] AE_CORE_API void ReflectionAbort(
        EReflectionErrorCode errorCode, const FReflectionDumpData& dumpData) {
        switch (errorCode) {
            case EReflectionErrorCode::TypeNotCopyConstructible:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Type is not copy constructible"));
                break;
            case EReflectionErrorCode::TypeNotDestructible:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Type is not destructible"));
                break;
            case EReflectionErrorCode::CorruptedAnyCast:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Corrupted Any cast operation"));
                break;
            case EReflectionErrorCode::MismatchedArgumentNumber:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Mismatched argument number"));
                break;
            case EReflectionErrorCode::TypeHashConflict:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Type hash conflict detected"));
                break;
            case EReflectionErrorCode::TypeUnregistered:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Type is not registered"));
                break;
            case EReflectionErrorCode::ObjectAndTypeMismatch:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Object and type metadata mismatch"));
                break;
            case EReflectionErrorCode::PropertyUnregistered:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Property is not registered"));
                break;
            case EReflectionErrorCode::DereferenceNullptr:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Dereferencing null pointer"));
                break;
            case EReflectionErrorCode::DeserializeCorruptedArchive:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Deserializing corrupted archive"));
                break;
            default:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Unknown error"));
        }

        LogDumpData(dumpData);
        std::abort();
    }
} // namespace AltinaEngine::Core::Reflection
