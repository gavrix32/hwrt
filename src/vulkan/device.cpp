#include <spdlog/spdlog.h>

#include "adapter.h"
#include "device.h"

Device::Device(const Adapter& adapter, const std::vector<const char*>& required_extensions) : handle(nullptr),
    queue(nullptr), queue_family_index(0) {

    auto queue_family_properties = adapter.get().getQueueFamilyProperties();

    size_t queue_fam_i = 0;
    for (auto queue_family_property : queue_family_properties) {
        std::string flag_names;

        auto flags = queue_family_property.queueFlags;
        if (flags & vk::QueueFlagBits::eGraphics) flag_names += "Graphics | ";
        if (flags & vk::QueueFlagBits::eCompute) flag_names += "Compute | ";
        if (flags & vk::QueueFlagBits::eTransfer) flag_names += "Transfer | ";
        if (flags & vk::QueueFlagBits::eSparseBinding) flag_names += "SparseBinding | ";
        if (flags & vk::QueueFlagBits::eProtected) flag_names += "Protected | ";
        if (flags & vk::QueueFlagBits::eVideoDecodeKHR) flag_names += "VideoDecodeKHR | ";
        if (flags & vk::QueueFlagBits::eVideoEncodeKHR) flag_names += "VideoEncodeKHR | ";
        if (flags & vk::QueueFlagBits::eOpticalFlowNV) flag_names += "OpticalFlowNV | ";
        if (flags & vk::QueueFlagBits::eDataGraphARM) flag_names += "DataGraphARM | ";

        spdlog::debug("queue_family_index: {}, queue_count: {}, queue_flags: {}",
                      queue_fam_i,
                      queue_family_property.queueCount,
                      flag_names);

        if (flags & vk::QueueFlagBits::eCompute && (flags & vk::QueueFlagBits::eGraphics)) {
            queue_family_index = queue_fam_i;
        }

        ++queue_fam_i;
    }
    spdlog::info("Selected queue family {}", queue_family_index);

    float queue_priority = 1.0f;
    vk::DeviceQueueCreateInfo device_queue_create_info{
        .queueFamilyIndex = queue_family_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };

    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceVulkan14Features,
                       vk::PhysicalDeviceBufferDeviceAddressFeatures, vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
                       vk::PhysicalDeviceRayTracingPipelineFeaturesKHR, vk::PhysicalDeviceShaderClockFeaturesKHR> features_chain;

    features_chain.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 = vk::True;
    features_chain.get<vk::PhysicalDeviceVulkan14Features>().pushDescriptor = vk::True;
    features_chain.get<vk::PhysicalDeviceBufferDeviceAddressFeatures>().bufferDeviceAddress = vk::True;
    features_chain.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>().accelerationStructure = vk::True;
    features_chain.get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>().rayTracingPipeline = vk::True;
    features_chain.get<vk::PhysicalDeviceFeatures2>().features.shaderInt64 = vk::True;
    features_chain.get<vk::PhysicalDeviceShaderClockFeaturesKHR>().shaderDeviceClock = vk::True;
    features_chain.get<vk::PhysicalDeviceShaderClockFeaturesKHR>().shaderSubgroupClock = vk::True;

    vk::DeviceCreateInfo device_create_info{
        .pNext = &features_chain.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &device_queue_create_info,
        .enabledExtensionCount = static_cast<uint32_t>(required_extensions.size()),
        .ppEnabledExtensionNames = required_extensions.data(),
    };

    handle = vk::raii::Device(adapter.get(), device_create_info);
    queue = vk::raii::Queue(handle, queue_family_index, 0);
}

const vk::raii::Device& Device::get() const {
    return handle;
}

const vk::raii::Queue& Device::get_queue() const {
    return queue;
}

uint32_t Device::get_queue_family_index() const {
    return queue_family_index;
}