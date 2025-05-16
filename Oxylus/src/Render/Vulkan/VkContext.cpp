#include "VkContext.hpp"

#include <vuk/ImageAttachment.hpp>
#include <vuk/RenderGraph.hpp>
#include <vuk/runtime/ThisThreadExecutor.hpp>
#include <vuk/runtime/vk/PipelineInstance.hpp>
#include <vuk/runtime/vk/Query.hpp>

#include "Render/RendererConfig.hpp"
#include "Render/Window.hpp"

namespace ox {
static VkBool32 debug_callback(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                               VkDebugUtilsMessageTypeFlagsEXT messageType,
                               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                               void* pUserData) {
  std::string prefix;
  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    prefix = "VULKAN VERBOSE: ";
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    prefix = "VULKAN INFO: ";
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    prefix = "VULKAN WARNING: ";
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    prefix = "VULKAN ERROR: ";
  }

  std::stringstream debug_message;
  debug_message << prefix << "[" << pCallbackData->messageIdNumber << "][" << pCallbackData->pMessageIdName
                << "] : " << pCallbackData->pMessage;

  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    OX_LOG_INFO("{}", debug_message.str());
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    OX_LOG_INFO("{}", debug_message.str());
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    OX_LOG_WARN(debug_message.str().c_str());
    // OX_DEBUGBREAK();
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    OX_LOG_FATAL("{}", debug_message.str());
  }
  return VK_FALSE;
}

vuk::Swapchain make_swapchain(vuk::Allocator& allocator,
                              vkb::Device& vkbdevice,
                              VkSurfaceKHR surface,
                              option<vuk::Swapchain> old_swapchain,
                              vuk::PresentModeKHR present_mode) {
  vkb::SwapchainBuilder swb(vkbdevice, surface);
  swb.set_desired_format(
         vuk::SurfaceFormatKHR{.format = vuk::Format::eR8G8B8A8Srgb, .colorSpace = vuk::ColorSpaceKHR::eSrgbNonlinear})
      .add_fallback_format(
          vuk::SurfaceFormatKHR{.format = vuk::Format::eB8G8R8A8Srgb, .colorSpace = vuk::ColorSpaceKHR::eSrgbNonlinear})
      .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
      .set_desired_present_mode(static_cast<VkPresentModeKHR>(present_mode));

  bool recycling = false;
  vkb::Result vkswapchain = {vkb::Swapchain{}};
  if (!old_swapchain) {
    vkswapchain = swb.build();
    old_swapchain.emplace(allocator, vkswapchain->image_count);
  } else {
    recycling = true;
    swb.set_old_swapchain(old_swapchain->swapchain);
    vkswapchain = swb.build();
  }

  if (recycling) {
    allocator.deallocate(std::span{&old_swapchain->swapchain, 1});
    for (auto& iv : old_swapchain->images) {
      allocator.deallocate(std::span{&iv.image_view, 1});
    }
  }

  auto images = *vkswapchain->get_images();
  auto views = *vkswapchain->get_image_views();

  old_swapchain->images.clear();

  for (uint32_t i = 0; i < (uint32_t)images.size(); i++) {
    vuk::ImageAttachment attachment = {
        .image = vuk::Image{.image = images[i], .allocation = nullptr},
        .image_view = vuk::ImageView{{0}, views[i]},
        .extent = {.width = vkswapchain->extent.width, .height = vkswapchain->extent.height, .depth = 1},
        .format = static_cast<vuk::Format>(vkswapchain->image_format),
        .sample_count = vuk::Samples::e1,
        .view_type = vuk::ImageViewType::e2D,
        .components = {},
        .base_level = 0,
        .level_count = 1,
        .base_layer = 0,
        .layer_count = 1,
    };
    old_swapchain->images.push_back(attachment);
  }

  old_swapchain->swapchain = vkswapchain->swapchain;
  old_swapchain->surface = surface;

  return std::move(*old_swapchain);
}

VkContext::~VkContext() { runtime->wait_idle(); }

void VkContext::handle_resize(u32 width,
                              u32 height) {
  if (width == 0 && height == 0) {
    suspend = true;
  } else {
    swapchain =
        make_swapchain(*superframe_allocator, vkb_device, swapchain->surface, std::move(swapchain), present_mode);
  }
}

void VkContext::set_vsync(bool enable) {
  const auto set_present_mode = enable ? vuk::PresentModeKHR::eFifo : vuk::PresentModeKHR::eImmediate;
  present_mode = set_present_mode;
}

bool VkContext::is_vsync() const { return present_mode == vuk::PresentModeKHR::eFifo; }

void VkContext::create_context(const Window& window,
                               bool vulkan_validation_layers) {
  OX_SCOPED_ZONE;
  vkb::InstanceBuilder builder;
  builder //
      .set_app_name("Oxylus App")
      .set_engine_name("Oxylus")
      .require_api_version(1, 3, 0)
      .set_app_version(0, 1, 0);

  if (vulkan_validation_layers) {
    OX_LOG_INFO("Enabled vulkan validation layers.");
    builder.request_validation_layers().set_debug_callback(
        [](const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
           const VkDebugUtilsMessageTypeFlagsEXT messageType,
           const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
           void* pUserData) -> VkBool32 {
      return debug_callback(messageSeverity, messageType, pCallbackData, pUserData);
    });
  }

  std::vector<const c8*> instance_extensions;
  instance_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
  instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  builder.enable_extensions(instance_extensions);

  auto inst_ret = builder.build();
  if (!inst_ret) {
    OX_LOG_ERROR(
        "Couldn't initialize the instance! Make sure your GPU drivers are up to date and it supports Vulkan 1.3");
  }

  vkb_instance = inst_ret.value();
  auto instance = vkb_instance.instance;
  vkb::PhysicalDeviceSelector selector{vkb_instance};
  surface = window.get_surface(instance);
  selector //
      .set_surface(surface)
      .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
      .set_minimum_version(1, 3);

  std::vector<const c8*> device_extensions;
  device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  device_extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
  device_extensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
  device_extensions.push_back(VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME);
  // device_extensions.push_back(VK_KHR_MAINTENANCE_8_EXTENSION_NAME);
  selector.add_required_extensions(device_extensions);

  if (auto phys_ret = selector.select(); !phys_ret) {
    OX_LOG_ERROR("{}", phys_ret.full_error().type.message());
  } else {
    vkbphysical_device = phys_ret.value();
    device_name = phys_ret.value().name;
  }

  physical_device = vkbphysical_device.physical_device;
  vkb::DeviceBuilder device_builder{vkbphysical_device};

  VkPhysicalDeviceFeatures2 vk10_features{};
  vk10_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  vk10_features.features.shaderInt64 = true;
  vk10_features.features.vertexPipelineStoresAndAtomics = true;
  vk10_features.features.depthClamp = true;
  vk10_features.features.fillModeNonSolid = true;
  vk10_features.features.multiViewport = true;
  vk10_features.features.samplerAnisotropy = true;
  vk10_features.features.multiDrawIndirect = true;

  VkPhysicalDeviceVulkan11Features vk11_features{};
  vk11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  vk11_features.shaderDrawParameters = true;
  vk11_features.variablePointers = true;
  vk11_features.variablePointersStorageBuffer = true;

  VkPhysicalDeviceVulkan12Features vk12_features{};
  vk12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  vk12_features.descriptorIndexing = true;
  vk12_features.shaderOutputLayer = true;
  vk12_features.shaderSampledImageArrayNonUniformIndexing = true;
  vk12_features.shaderStorageBufferArrayNonUniformIndexing = true;
  vk12_features.descriptorBindingSampledImageUpdateAfterBind = true;
  vk12_features.descriptorBindingStorageImageUpdateAfterBind = true;
  vk12_features.descriptorBindingStorageBufferUpdateAfterBind = true;
  vk12_features.descriptorBindingUpdateUnusedWhilePending = true;
  vk12_features.descriptorBindingPartiallyBound = true;
  vk12_features.descriptorBindingVariableDescriptorCount = true;
  vk12_features.runtimeDescriptorArray = true;
  vk12_features.timelineSemaphore = true;
  vk12_features.bufferDeviceAddress = true;
  vk12_features.hostQueryReset = true;
  // Shader features
  vk12_features.vulkanMemoryModel = true;
  vk12_features.storageBuffer8BitAccess = true;
  vk12_features.scalarBlockLayout = true;
  vk12_features.shaderInt8 = true;
  vk12_features.shaderSubgroupExtendedTypes = true;

  VkPhysicalDeviceVulkan13Features vk13_features = {};
  vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  vk13_features.synchronization2 = true;
  vk13_features.shaderDemoteToHelperInvocation = true;

  VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT image_atomic_int64_features = {};
  image_atomic_int64_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT;
  image_atomic_int64_features.shaderImageInt64Atomics = true;

  VkPhysicalDeviceVulkan14Features vk14_features = {};
  vk14_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
  vk14_features.pushDescriptor = true;
  device_builder //
      .add_pNext(&vk14_features)
      .add_pNext(&vk13_features)
      .add_pNext(&vk12_features)
      .add_pNext(&vk11_features)
      .add_pNext(&image_atomic_int64_features)
      .add_pNext(&vk10_features);

  auto dev_ret = device_builder.build();
  if (!dev_ret) {
    OX_LOG_ERROR("Couldn't create device");
  }

  vkb_device = dev_ret.value();
  graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
  graphics_queue_family_index = vkb_device.get_queue_index(vkb::QueueType::graphics).value();
  transfer_queue = vkb_device.get_queue(vkb::QueueType::transfer).value();
  auto transfer_queue_family_index = vkb_device.get_queue_index(vkb::QueueType::transfer).value();
  device = vkb_device.device;
  vuk::FunctionPointers fps;
  fps.vkGetInstanceProcAddr = vkb_instance.fp_vkGetInstanceProcAddr;
  fps.vkGetDeviceProcAddr = vkb_instance.fp_vkGetDeviceProcAddr;
  fps.load_pfns(instance, device, true);
  std::vector<std::unique_ptr<vuk::Executor>> executors;

  executors.push_back(create_vkqueue_executor(fps,
                                              device,
                                              graphics_queue,
                                              graphics_queue_family_index,
                                              vuk::DomainFlagBits::eGraphicsQueue));
  executors.push_back(create_vkqueue_executor(fps,
                                              device,
                                              transfer_queue,
                                              transfer_queue_family_index,
                                              vuk::DomainFlagBits::eTransferQueue));
  executors.push_back(std::make_unique<vuk::ThisThreadExecutor>());

  runtime.emplace(vuk::RuntimeCreateParameters{instance, device, physical_device, std::move(executors), fps});

  set_vsync(static_cast<bool>(RendererCVar::cvar_vsync.get()));

  superframe_resource.emplace(*runtime, num_inflight_frames);
  superframe_allocator.emplace(*superframe_resource);
  swapchain = make_swapchain(*superframe_allocator, vkb_device, surface, {}, present_mode);
  present_ready = vuk::Unique<std::array<VkSemaphore, 3>>(*superframe_allocator);
  render_complete = vuk::Unique<std::array<VkSemaphore, 3>>(*superframe_allocator);

  // runtime->set_shader_target_version(VK_API_VERSION_1_3);

  shader_compiler = SlangCompiler::create().value();

  superframe_allocator->allocate_semaphores(*present_ready);
  superframe_allocator->allocate_semaphores(*render_complete);

  tracy_profiler = create_shared<TracyProfiler>();
  tracy_profiler->init_tracy_for_vulkan(this);

  OX_LOG_INFO("Vulkan context initialized using device: {}", device_name);
}

vuk::Value<vuk::ImageAttachment> VkContext::new_frame() {
  OX_SCOPED_ZONE;
  runtime->next_frame();

  auto acquired_swapchain = vuk::acquire_swapchain(*swapchain);
  auto acquired_image = vuk::acquire_next_image("present_image", std::move(acquired_swapchain));

  auto& frame_resource = superframe_resource->get_next_frame();
  frame_allocator = vuk::Allocator(frame_resource);

  return acquired_image;
}

void VkContext::end_frame(vuk::Allocator& frame_allocator_,
                          vuk::Value<vuk::ImageAttachment> target_) {
  auto entire_thing = vuk::enqueue_presentation(std::move(target_));
  vuk::ProfilingCallbacks cbs = tracy_profiler->setup_vuk_callback();
  entire_thing.submit(frame_allocator_, compiler, {.graph_label = {}, .callbacks = cbs});

  current_frame = (current_frame + 1) % num_inflight_frames;
  num_frames = runtime->get_frame_count();
}

option<vuk::Allocator>& VkContext::get_frame_allocator() { return this->frame_allocator; }

} // namespace ox
