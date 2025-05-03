#pragma once

#include <vuk/ImageAttachment.hpp>
#include <vuk/RenderGraph.hpp>
#include <vuk/Value.hpp>
#include <vuk/runtime/vk/PipelineInstance.hpp>
#include <vuk/runtime/vk/Query.hpp>

using Preset = vuk::ImageAttachment::Preset;

namespace ox {
struct TextureLoadInfo {
  Preset preset = Preset::eMap2D;
  vuk::Format format = vuk::Format::eR8G8B8A8Unorm;
  enum class MimeType { Generic, KTX } mime = MimeType::Generic;
  void* data = nullptr; // optional
  vuk::Extent2D extent = {}; // optional
};

enum class TextureID : uint64 { Invalid = std::numeric_limits<uint64>::max() };
class Texture {
public:
  Texture() = default;
  ~Texture() = default;

  Texture& operator=(Texture&& other) noexcept {
    if (this != &other) {
      _image = std::move(other._image);
      _view = std::move(other._view);
      _attachment = std::move(other._attachment);
      _name = std::move(other._name);
    }
    return *this;
  }

  Texture(Texture&& other) noexcept { *this = std::move(other); }

  auto create(const std::string& path,
              const TextureLoadInfo& load_info,
              const std::source_location& loc = std::source_location::current()) -> void;

  auto attachment() const -> vuk::ImageAttachment { return _attachment; }
  auto acquire(vuk::Name name = {},
               vuk::Access last_access = vuk::Access::eFragmentSampled) const -> vuk::Value<vuk::ImageAttachment>;
  auto discard(vuk::Name name = {}) const -> vuk::Value<vuk::ImageAttachment>;

  auto get_image() const -> const vuk::Unique<vuk::Image>& { return _image; }
  auto get_view() const -> const vuk::Unique<vuk::ImageView>& { return _view; }
  auto get_extent() const -> const vuk::Extent3D& { return _attachment.extent; }

  auto set_name(std::string_view name,
                const std::source_location& loc = std::source_location::current()) -> void;

  auto get_view_id() const -> uint64 { return _view->id; }

  static auto load_stb_image(const std::string& filename,
                             uint32_t* width = nullptr,
                             uint32_t* height = nullptr,
                             uint32_t* bits = nullptr,
                             bool srgb = true) -> Unique<uint8[]>;

  static auto load_stb_image_from_memory(void* buffer,
                                         size_t len,
                                         uint32_t* width = nullptr,
                                         uint32_t* height = nullptr,
                                         uint32_t* bits = nullptr,
                                         bool flipY = false,
                                         bool srgb = true) -> Unique<uint8[]>;

  static auto get_magenta_texture(uint32_t width,
                                  uint32_t height,
                                  uint32_t channels) -> uint8_t*;

  static auto convert_to_four_channels(uint32_t width,
                                       uint32_t height,
                                       const uint8_t* three_channel_data) -> uint8_t*;

  static auto get_mip_count(const vuk::Extent3D extent) -> uint32_t {
    return static_cast<uint32_t>(log2f(static_cast<float>(std::max(std::max(extent.width, extent.height), extent.depth)))) + 1;
  }

private:
  vuk::ImageAttachment _attachment = {};
  vuk::Unique<vuk::Image> _image;
  vuk::Unique<vuk::ImageView> _view;
  std::string _name = {};
};
} // namespace ox
