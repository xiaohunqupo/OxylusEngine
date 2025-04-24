#pragma once

#include "Core/Base.hpp"

#include <optional>

#include <VkBootstrap.h>
#include <vuk/RenderGraph.hpp>
#include <vuk/Value.hpp>
#include <vuk/runtime/vk/DeviceFrameResource.hpp>
#include <vuk/runtime/vk/VkRuntime.hpp>

#include "Core/Option.hpp"
#include "Core/Types.hpp"
#include "Utils/Profiler.hpp"

#include <Render/Slang/Compiler.hpp>

namespace ox {
struct Window;

class VkContext {
public:
  VkDevice device = nullptr;
  VkPhysicalDevice physical_device = nullptr;
  vkb::PhysicalDevice vkbphysical_device;
  VkQueue graphics_queue = nullptr;
  uint32_t graphics_queue_family_index = 0;
  VkQueue transfer_queue = nullptr;
  std::optional<vuk::Runtime> runtime;
  std::optional<vuk::DeviceSuperFrameResource> superframe_resource;
  std::optional<vuk::Allocator> superframe_allocator;
  bool suspend = false;
  vuk::PresentModeKHR present_mode = vuk::PresentModeKHR::eFifo;
  std::optional<vuk::Swapchain> swapchain;
  VkSurfaceKHR surface;
  vkb::Instance vkb_instance;
  vkb::Device vkb_device;
  uint32_t num_inflight_frames = 3;
  uint64_t num_frames = 0;
  uint32_t current_frame = 0;
  vuk::Unique<std::array<VkSemaphore, 3>> present_ready;
  vuk::Unique<std::array<VkSemaphore, 3>> render_complete;
  Shared<TracyProfiler> tracy_profiler = {};
  vuk::Compiler compiler = {};
  option<vuk::Allocator> frame_allocator = nullopt;
  SlangCompiler shader_compiler = {};

  std::string device_name = {};

  VkContext() = default;

  void create_context(const Window& window, bool vulkan_validation_layers);
  vuk::Value<vuk::ImageAttachment> new_frame();
  void end_frame(vuk::Allocator& frame_allocator, vuk::Value<vuk::ImageAttachment> target);
  void handle_resize(uint32 width, uint32 height);
  void set_vsync(bool enable);
  bool is_vsync() const;
  option<vuk::Allocator>& get_frame_allocator();

  uint32_t get_max_viewport_count() const { return vkbphysical_device.properties.limits.maxViewports; }
};
}
