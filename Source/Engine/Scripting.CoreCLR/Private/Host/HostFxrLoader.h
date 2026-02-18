#pragma once

#include "Base/AltinaBase.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Types/Aliases.h"

#include <filesystem>

namespace AltinaEngine::Scripting::CoreCLR::Host {
    namespace Container = ::AltinaEngine::Core::Container;
#if AE_PLATFORM_WIN
    using FHostFxrChar = wchar_t;
    #define AE_HOSTFXR_CALLTYPE __stdcall
    #define AE_CORECLR_CALLTYPE __cdecl
#else
    using FHostFxrChar = char;
    #define AE_HOSTFXR_CALLTYPE
    #define AE_CORECLR_CALLTYPE
#endif

    using FHostFxrString = std::basic_string<FHostFxrChar>;

    using hostfxr_handle = void*;

    enum class EHostFxrDelegateType : i32 {
        LoadAssemblyAndGetFunctionPointer = 5
    };

    struct FHostFxrInitializeParameters {
        usize              mSize;
        const FHostFxrChar* mHostPath;
        const FHostFxrChar* mDotnetRoot;
    };

    using hostfxr_initialize_for_runtime_config_fn = i32 (AE_HOSTFXR_CALLTYPE*)(
        const FHostFxrChar* runtimeConfigPath, const FHostFxrInitializeParameters* parameters,
        hostfxr_handle* outHandle);

    using hostfxr_get_runtime_delegate_fn = i32 (AE_HOSTFXR_CALLTYPE*)(
        hostfxr_handle handle, EHostFxrDelegateType type, void** delegate);

    using hostfxr_close_fn = i32 (AE_HOSTFXR_CALLTYPE*)(hostfxr_handle handle);

    using load_assembly_and_get_function_pointer_fn = i32 (AE_CORECLR_CALLTYPE*)(
        const FHostFxrChar* assemblyPath, const FHostFxrChar* typeName,
        const FHostFxrChar* methodName, const FHostFxrChar* delegateTypeName, void* reserved,
        void** delegate);

    using component_entry_point_fn = i32 (AE_CORECLR_CALLTYPE*)(void* args, i32 size);

    using get_hostfxr_path_fn = i32 (AE_HOSTFXR_CALLTYPE*)(
        FHostFxrChar* buffer, usize* bufferSize, const FHostFxrChar* assemblyPath);

    struct FHostFxrFunctions {
        hostfxr_initialize_for_runtime_config_fn InitializeForRuntimeConfig = nullptr;
        hostfxr_get_runtime_delegate_fn          GetRuntimeDelegate          = nullptr;
        hostfxr_close_fn                         Close                       = nullptr;
    };

    struct FDynamicLibrary {
        ~FDynamicLibrary() { Unload(); }

        auto Load(const std::filesystem::path& path) -> bool;
        void Unload();
        [[nodiscard]] auto GetSymbol(const char* name) const -> void*;
        [[nodiscard]] auto IsLoaded() const noexcept -> bool { return mHandle != nullptr; }

        void* mHandle = nullptr;
    };

    auto ToHostFxrString(Container::FStringView value) -> FHostFxrString;

    class FHostFxrLibrary {
    public:
        auto Load(const Container::FString& runtimeConfigPath, const Container::FString& runtimeRoot,
            const Container::FString& dotnetRoot) -> bool;
        void Unload();

        [[nodiscard]] auto IsLoaded() const noexcept -> bool { return mLibrary.IsLoaded(); }
        [[nodiscard]] auto GetFunctions() const noexcept -> const FHostFxrFunctions& {
            return mFunctions;
        }
        [[nodiscard]] auto GetDotnetRoot() const noexcept -> const FHostFxrString& {
            return mDotnetRoot;
        }
        [[nodiscard]] auto GetHostFxrPath() const noexcept -> const FHostFxrString& {
            return mHostFxrPath;
        }

    private:
        FDynamicLibrary  mLibrary;
        FHostFxrFunctions mFunctions{};
        FHostFxrString    mDotnetRoot;
        FHostFxrString    mHostFxrPath;
    };
} // namespace AltinaEngine::Scripting::CoreCLR::Host

#undef AE_HOSTFXR_CALLTYPE
#undef AE_CORECLR_CALLTYPE
