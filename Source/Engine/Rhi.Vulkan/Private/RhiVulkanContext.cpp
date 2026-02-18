#include "RhiVulkan/RhiVulkanContext.h"

#include "RhiVulkan/RhiVulkanDevice.h"
#include "Container/SmartPtr.h"
#include "Logging/Log.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"

#include "RhiVulkanInternal.h"

#include <type_traits>

using AltinaEngine::Forward;
using AltinaEngine::Move;
using AltinaEngine::Core::Container::TAllocator;
using AltinaEngine::Core::Container::TAllocatorTraits;
namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::MakeUnique;
    using Container::TVector;

#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    struct FRhiVulkanContextState {
        VkInstance                mInstance        = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT  mDebugMessenger  = VK_NULL_HANDLE;
        u32                       mInstanceVersion = VK_API_VERSION_1_0;
        bool                      mDebugUtilsEnabled = false;
        TVector<const char*>      mEnabledLayers;
        TVector<const char*>      mEnabledExtensions;
    };
#else
    struct FRhiVulkanContextState {};
#endif

    namespace {
        template <typename TBase, typename TDerived, typename... Args>
        auto MakeSharedAs(Args&&... args) -> TShared<TBase> {
            using TAllocatorType = TAllocator<TDerived>;
            using Traits         = TAllocatorTraits<TAllocatorType>;

            static_assert(std::is_base_of_v<TBase, TDerived>,
                "MakeSharedAs requires TDerived to derive from TBase.");

            TAllocatorType allocator;
            TDerived*      ptr = Traits::Allocate(allocator, 1);
            try {
                Traits::Construct(allocator, ptr, Forward<Args>(args)...);
            } catch (...) {
                Traits::Deallocate(allocator, ptr, 1);
                throw;
            }

            struct FDeleter {
                TAllocatorType mAllocator;
                void           operator()(TBase* basePtr) {
                    if (!basePtr) {
                        return;
                    }
                    auto* derivedPtr = AltinaEngine::CheckedCast<TDerived*>(basePtr);
                    Traits::Destroy(mAllocator, derivedPtr);
                    Traits::Deallocate(mAllocator, derivedPtr, 1);
                }
            };

            return TShared<TBase>(ptr, FDeleter{ allocator });
        }

#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        auto GetVulkanVersion() -> u32 {
            u32 version = VK_API_VERSION_1_0;
            if (vkEnumerateInstanceVersion) {
                vkEnumerateInstanceVersion(&version);
            }
            return version;
        }

        auto PickApiVersion(u32 available) -> u32 {
#if defined(VK_API_VERSION_1_4)
            const u32 preferred = VK_API_VERSION_1_4;
#elif defined(VK_API_VERSION_1_3)
            const u32 preferred = VK_API_VERSION_1_3;
#else
            const u32 preferred = VK_API_VERSION_1_2;
#endif
            return (available < preferred) ? available : preferred;
        }

        auto HasLayer(const TVector<VkLayerProperties>& layers, const char* name) -> bool {
            if (!name) {
                return false;
            }
            for (const auto& layer : layers) {
                if (std::strcmp(layer.layerName, name) == 0) {
                    return true;
                }
            }
            return false;
        }

        auto HasExtension(const TVector<VkExtensionProperties>& exts, const char* name) -> bool {
            if (!name) {
                return false;
            }
            for (const auto& ext : exts) {
                if (std::strcmp(ext.extensionName, name) == 0) {
                    return true;
                }
            }
            return false;
        }

        VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT,
            const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
            void*) {
            if (!callbackData || !callbackData->pMessage) {
                return VK_FALSE;
            }
            if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                LogWarning(TEXT("Vulkan: {}"), callbackData->pMessage);
            } else {
                LogInfo(TEXT("Vulkan: {}"), callbackData->pMessage);
            }
            return VK_FALSE;
        }

        auto CreateDebugMessenger(VkInstance instance) -> VkDebugUtilsMessengerEXT {
            VkDebugUtilsMessengerCreateInfoEXT info{};
            info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            info.pfnUserCallback = DebugCallback;

            auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
            if (!createFn) {
                return VK_NULL_HANDLE;
            }

            VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
            if (createFn(instance, &info, nullptr, &messenger) != VK_SUCCESS) {
                return VK_NULL_HANDLE;
            }
            return messenger;
        }

        void DestroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger) {
            if (!messenger) {
                return;
            }
            auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroyFn) {
                destroyFn(instance, messenger, nullptr);
            }
        }

        auto MapAdapterType(VkPhysicalDeviceType type) noexcept -> ERhiAdapterType {
            switch (type) {
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                    return ERhiAdapterType::Discrete;
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                    return ERhiAdapterType::Integrated;
                case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                    return ERhiAdapterType::Virtual;
                case VK_PHYSICAL_DEVICE_TYPE_CPU:
                    return ERhiAdapterType::Cpu;
                default:
                    return ERhiAdapterType::Unknown;
            }
        }

        auto MapVendorId(u32 vendor) noexcept -> ERhiVendorId {
            switch (vendor) {
                case static_cast<u32>(ERhiVendorId::Nvidia):
                    return ERhiVendorId::Nvidia;
                case static_cast<u32>(ERhiVendorId::AMD):
                    return ERhiVendorId::AMD;
                case static_cast<u32>(ERhiVendorId::Intel):
                    return ERhiVendorId::Intel;
                case static_cast<u32>(ERhiVendorId::Microsoft):
                    return ERhiVendorId::Microsoft;
                default:
                    return ERhiVendorId::Unknown;
            }
        }

        void FillAdapterMemoryDesc(VkPhysicalDevice physical, FRhiAdapterDesc& desc) {
            VkPhysicalDeviceMemoryProperties memProps{};
            vkGetPhysicalDeviceMemoryProperties(physical, &memProps);
            u64 deviceLocal = 0ULL;
            u64 shared      = 0ULL;
            for (u32 i = 0; i < memProps.memoryHeapCount; ++i) {
                const auto& heap = memProps.memoryHeaps[i];
                if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                    deviceLocal += heap.size;
                } else {
                    shared += heap.size;
                }
            }
            desc.mDedicatedVideoMemoryBytes = deviceLocal;
            desc.mSharedSystemMemoryBytes   = shared;
        }

        class FRhiVulkanAdapter final : public FRhiAdapter {
        public:
            FRhiVulkanAdapter(const FRhiAdapterDesc& desc, VkPhysicalDevice physical)
                : FRhiAdapter(desc), mPhysical(physical) {}

            [[nodiscard]] auto GetPhysicalDevice() const noexcept -> VkPhysicalDevice {
                return mPhysical;
            }

        private:
            VkPhysicalDevice mPhysical = VK_NULL_HANDLE;
        };
#endif
    } // namespace

    FRhiVulkanContext::FRhiVulkanContext() {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        mState = MakeUnique<FRhiVulkanContextState>();
#endif
    }

    FRhiVulkanContext::~FRhiVulkanContext() {
        Shutdown();
        mState.Reset();
    }

    auto FRhiVulkanContext::AdjustProjectionMatrix(
        const Core::Math::FMatrix4x4f& matrix) const noexcept -> Core::Math::FMatrix4x4f {
        Core::Math::FMatrix4x4f result = matrix;
        result.m[1][1] *= -1.0f;
        return result;
    }

    auto FRhiVulkanContext::InitializeBackend(const FRhiInitDesc& desc) -> bool {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        if (!mState) {
            mState = MakeUnique<FRhiVulkanContextState>();
        }

        LogInfo(TEXT("RHI(Vulkan): Initializing (DebugLayer={}, GPUValidation={})."),
            desc.mEnableDebugLayer, desc.mEnableGpuValidation);

        const u32 loaderVersion = GetVulkanVersion();
        mState->mInstanceVersion = PickApiVersion(loaderVersion);

        VkApplicationInfo appInfo{};
        appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.apiVersion         = mState->mInstanceVersion;
        appInfo.applicationVersion = desc.mAppVersion;
        appInfo.engineVersion      = desc.mEngineVersion;
        if (!desc.mAppName.IsEmptyString()) {
            appInfo.pApplicationName = desc.mAppName.CStr();
        }
        appInfo.pEngineName = "AltinaEngine";

        TVector<VkLayerProperties> layers;
        u32 layerCount = 0;
        if (vkEnumerateInstanceLayerProperties(&layerCount, nullptr) == VK_SUCCESS
            && layerCount > 0) {
            layers.Resize(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, layers.Data());
        }

        if (desc.mEnableDebugLayer || desc.mEnableGpuValidation) {
            const char* validationLayer = "VK_LAYER_KHRONOS_validation";
            if (HasLayer(layers, validationLayer)) {
                mState->mEnabledLayers.PushBack(validationLayer);
            }
        }

        TVector<VkExtensionProperties> exts;
        u32 extCount = 0;
        if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr) == VK_SUCCESS
            && extCount > 0) {
            exts.Resize(extCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &extCount, exts.Data());
        }

        mState->mEnabledExtensions.PushBack(VK_KHR_SURFACE_EXTENSION_NAME);
#if AE_PLATFORM_WIN
        mState->mEnabledExtensions.PushBack(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
        if ((desc.mEnableDebugLayer || desc.mEnableDebugNames) &&
            HasExtension(exts, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            mState->mEnabledExtensions.PushBack(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            mState->mDebugUtilsEnabled = true;
        }

        VkInstanceCreateInfo createInfo{};
        createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo        = &appInfo;
        createInfo.enabledLayerCount       = static_cast<u32>(mState->mEnabledLayers.Size());
        createInfo.ppEnabledLayerNames     = mState->mEnabledLayers.Data();
        createInfo.enabledExtensionCount   = static_cast<u32>(mState->mEnabledExtensions.Size());
        createInfo.ppEnabledExtensionNames = mState->mEnabledExtensions.Data();

        if (vkCreateInstance(&createInfo, nullptr, &mState->mInstance) != VK_SUCCESS) {
            LogError(TEXT("RHI(Vulkan): Failed to create VkInstance."));
            return false;
        }

        if (mState->mDebugUtilsEnabled) {
            mState->mDebugMessenger = CreateDebugMessenger(mState->mInstance);
        }

        LogInfo(TEXT("RHI(Vulkan): Instance created (API={}.{}.{})"),
            VK_VERSION_MAJOR(mState->mInstanceVersion),
            VK_VERSION_MINOR(mState->mInstanceVersion),
            VK_VERSION_PATCH(mState->mInstanceVersion));
        return true;
#else
        (void)desc;
        LogWarning(TEXT("RHI(Vulkan): Vulkan not available on this build."));
        return false;
#endif
    }

    void FRhiVulkanContext::ShutdownBackend() {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        if (!mState) {
            return;
        }
        if (mState->mDebugMessenger) {
            DestroyDebugMessenger(mState->mInstance, mState->mDebugMessenger);
            mState->mDebugMessenger = VK_NULL_HANDLE;
        }
        if (mState->mInstance) {
            vkDestroyInstance(mState->mInstance, nullptr);
            mState->mInstance = VK_NULL_HANDLE;
        }
        mState->mEnabledExtensions.Clear();
        mState->mEnabledLayers.Clear();
#endif
    }

    void FRhiVulkanContext::EnumerateAdaptersInternal(TVector<TShared<FRhiAdapter>>& outAdapters) {
        outAdapters.Clear();
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        if (!mState || mState->mInstance == VK_NULL_HANDLE) {
            return;
        }

        u32 deviceCount = 0;
        if (vkEnumeratePhysicalDevices(mState->mInstance, &deviceCount, nullptr) != VK_SUCCESS
            || deviceCount == 0) {
            return;
        }

        TVector<VkPhysicalDevice> devices;
        devices.Resize(deviceCount);
        if (vkEnumeratePhysicalDevices(mState->mInstance, &deviceCount, devices.Data())
            != VK_SUCCESS) {
            return;
        }

        outAdapters.Reserve(deviceCount);
        for (VkPhysicalDevice physical : devices) {
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(physical, &props);

            FRhiAdapterDesc desc;
            desc.mName.Assign(props.deviceName);
            desc.mVendorId  = MapVendorId(props.vendorID);
            desc.mDeviceId  = props.deviceID;
            desc.mType      = MapAdapterType(props.deviceType);
            desc.mApiVersion    = props.apiVersion;
            desc.mDriverVersion = props.driverVersion;
            FillAdapterMemoryDesc(physical, desc);

            outAdapters.PushBack(MakeSharedAs<FRhiAdapter, FRhiVulkanAdapter>(desc, physical));
        }
#else
        (void)outAdapters;
#endif
    }

    auto FRhiVulkanContext::CreateDeviceInternal(
        const TShared<FRhiAdapter>& adapter, const FRhiDeviceDesc& desc) -> TShared<FRhiDevice> {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        if (!mState || mState->mInstance == VK_NULL_HANDLE || !adapter) {
            return {};
        }

        const auto* vkAdapter = AltinaEngine::CheckedCast<const FRhiVulkanAdapter*>(adapter.Get());
        if (!vkAdapter) {
            return {};
        }

        VkPhysicalDevice physical = vkAdapter->GetPhysicalDevice();
        if (physical == VK_NULL_HANDLE) {
            return {};
        }

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        VkPhysicalDeviceFeatures2 features2{};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        VkPhysicalDeviceVulkan12Features features12{};
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        VkPhysicalDeviceDescriptorIndexingFeatures descIndex{};
        descIndex.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        VkPhysicalDeviceTimelineSemaphoreFeatures timeline{};
        timeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
        VkPhysicalDeviceSynchronization2Features sync2{};
        sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
        VkPhysicalDeviceDynamicRenderingFeatures dynRendering{};
        dynRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;

        features2.pNext = &features13;
        features13.pNext = &features12;
        features12.pNext = &descIndex;
        descIndex.pNext  = &timeline;
        timeline.pNext   = &sync2;
        sync2.pNext      = &dynRendering;

        vkGetPhysicalDeviceFeatures2(physical, &features2);

        features13.dynamicRendering = features13.dynamicRendering || dynRendering.dynamicRendering;
        sync2.synchronization2 = sync2.synchronization2;

        // Enable supported feature subset.
        VkPhysicalDeviceFeatures2 enabledFeatures = features2;
        enabledFeatures.pNext = &features13;
        features13.pNext      = &features12;
        features12.pNext      = &descIndex;
        descIndex.pNext       = &timeline;
        timeline.pNext        = &sync2;
        sync2.pNext           = &dynRendering;

        // Enable commonly used core features if supported.
        enabledFeatures.features.samplerAnisotropy = features2.features.samplerAnisotropy;
        enabledFeatures.features.fillModeNonSolid  = features2.features.fillModeNonSolid;

        // Descriptor indexing (bindless) support
        descIndex.shaderSampledImageArrayNonUniformIndexing =
            descIndex.shaderSampledImageArrayNonUniformIndexing;
        descIndex.shaderStorageBufferArrayNonUniformIndexing =
            descIndex.shaderStorageBufferArrayNonUniformIndexing;
        descIndex.descriptorBindingPartiallyBound = descIndex.descriptorBindingPartiallyBound;
        descIndex.descriptorBindingUpdateAfterBind =
            descIndex.descriptorBindingUpdateAfterBind;
        descIndex.runtimeDescriptorArray = descIndex.runtimeDescriptorArray;
        descIndex.descriptorBindingVariableDescriptorCount =
            descIndex.descriptorBindingVariableDescriptorCount;

        timeline.timelineSemaphore = timeline.timelineSemaphore;
        sync2.synchronization2     = sync2.synchronization2;
        dynRendering.dynamicRendering =
            dynRendering.dynamicRendering || features13.dynamicRendering;

        // Queue family selection
        u32 queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &queueFamilyCount, nullptr);
        TVector<VkQueueFamilyProperties> families;
        families.Resize(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &queueFamilyCount, families.Data());

        auto selectFamily = [&](VkQueueFlags required, VkQueueFlags preferExclusive) -> u32 {
            for (u32 i = 0; i < queueFamilyCount; ++i) {
                const auto& props = families[i];
                if ((props.queueFlags & required) != required) {
                    continue;
                }
                if ((props.queueFlags & preferExclusive) == required) {
                    return i;
                }
            }
            for (u32 i = 0; i < queueFamilyCount; ++i) {
                const auto& props = families[i];
                if ((props.queueFlags & required) == required) {
                    return i;
                }
            }
            return UINT32_MAX;
        };

        const u32 graphicsFamily = selectFamily(VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_GRAPHICS_BIT);
        u32 computeFamily = selectFamily(VK_QUEUE_COMPUTE_BIT, VK_QUEUE_COMPUTE_BIT);
        u32 transferFamily = selectFamily(VK_QUEUE_TRANSFER_BIT, VK_QUEUE_TRANSFER_BIT);

        if (graphicsFamily == UINT32_MAX) {
            LogError(TEXT("RHI(Vulkan): No graphics queue family found."));
            return {};
        }
        if (computeFamily == UINT32_MAX) {
            computeFamily = graphicsFamily;
        }
        if (transferFamily == UINT32_MAX) {
            transferFamily = (computeFamily != graphicsFamily) ? computeFamily : graphicsFamily;
        }

        const float queuePriority = 1.0f;
        TVector<VkDeviceQueueCreateInfo> queueInfos;
        auto addQueueInfo = [&](u32 family) {
            for (const auto& info : queueInfos) {
                if (info.queueFamilyIndex == family) {
                    return;
                }
            }
            VkDeviceQueueCreateInfo info{};
            info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            info.queueFamilyIndex = family;
            info.queueCount       = 1;
            info.pQueuePriorities = &queuePriority;
            queueInfos.PushBack(info);
        };

        addQueueInfo(graphicsFamily);
        addQueueInfo(computeFamily);
        addQueueInfo(transferFamily);

        TVector<const char*> deviceExtensions;
        deviceExtensions.PushBack(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        if (sync2.synchronization2) {
            deviceExtensions.PushBack(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
        }
        if (dynRendering.dynamicRendering) {
            deviceExtensions.PushBack(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        }
        if (timeline.timelineSemaphore) {
            deviceExtensions.PushBack(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
        }

        createInfo.pQueueCreateInfos = queueInfos.Data();
        createInfo.queueCreateInfoCount = static_cast<u32>(queueInfos.Size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.Data();
        createInfo.enabledExtensionCount   = static_cast<u32>(deviceExtensions.Size());
        createInfo.pNext                   = &enabledFeatures;

        VkDevice device = VK_NULL_HANDLE;
        if (vkCreateDevice(physical, &createInfo, nullptr, &device) != VK_SUCCESS) {
            LogError(TEXT("RHI(Vulkan): Failed to create VkDevice."));
            return {};
        }

        return MakeSharedAs<FRhiDevice, FRhiVulkanDevice>(
            desc, adapter->GetDesc(), mState->mInstance, physical, device);
#else
        (void)adapter;
        (void)desc;
        return {};
#endif
    }

} // namespace AltinaEngine::Rhi
