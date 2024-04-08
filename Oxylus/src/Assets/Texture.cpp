#include "Texture.hpp"

#include <filesystem>
#include <stb_image.h>
#include <vuk/Partials.hpp>

#include "Core/FileSystem.hpp"
#include "Render/RendererCommon.h"
#include "Render/Utils/VukCommon.hpp"
#include "Render/Vulkan/VkContext.hpp"
#include "Utils/Log.hpp"
#include "Utils/Profiler.hpp"

namespace ox {
Shared<Texture> Texture::_white_texture = nullptr;

Texture::Texture(const std::string& file_path) { load(TextureLoadInfo{.path = file_path}); }

Texture::Texture(const TextureLoadInfo& info) {
  if (!info.path.empty())
    load(info);
  else
    create_texture(info.extent, info.data, info.format);
}

Texture::~Texture() = default;

void Texture::create_texture(const vuk::Extent3D extent, vuk::Format format, vuk::ImageAttachment::Preset preset, std::source_location loc) {
  const auto ctx = VkContext::get();
  auto ia = vuk::ImageAttachment::from_preset(preset, format, extent, vuk::Samples::e1);
  ia.usage |= vuk::ImageUsageFlagBits::eTransferDst | vuk::ImageUsageFlagBits::eTransferSrc;
  auto image = vuk::allocate_image(*ctx->superframe_allocator, ia);
  ia.image = **image;
  auto view = vuk::allocate_image_view(*ctx->superframe_allocator, ia);

  _image = std::move(*image);
  _view = std::move(*view);
  _attachment = ia;

  set_name(loc);
}

void Texture::create_texture(const vuk::ImageAttachment& image_attachment, std::source_location loc) {
  const auto ctx = VkContext::get();
  auto ia = image_attachment;
  ia.usage |= vuk::ImageUsageFlagBits::eTransferDst;
  auto image = vuk::allocate_image(*ctx->superframe_allocator, ia);
  ia.image = **image;
  auto view = vuk::allocate_image_view(*ctx->superframe_allocator, ia);

  _image = std::move(*image);
  _view = std::move(*view);
  _attachment = ia;

  set_name(loc);
}

void Texture::create_texture(vuk::Extent3D extent, const void* data, const vuk::Format format, Preset preset, std::source_location loc) {
  OX_SCOPED_ZONE;
  const auto ctx = VkContext::get();

  auto ia = vuk::ImageAttachment::from_preset(preset, format, extent, vuk::Samples::e1);
  ia.usage |= vuk::ImageUsageFlagBits::eTransferDst | vuk::ImageUsageFlagBits::eTransferSrc;
  auto& alloc = *ctx->superframe_allocator;
  auto [tex, view, fut] = vuk::create_image_and_view_with_data(alloc, vuk::DomainFlagBits::eTransferOnTransfer, ia, data);

  if (ia.level_count > 1)
    fut = vuk::generate_mips(fut, ia.level_count);

  vuk::Compiler compiler;
  fut.as_released(vuk::eFragmentSampled).wait(*ctx->superframe_allocator, compiler);

  _image = std::move(tex);
  _view = std::move(view);
  _attachment = ia;

  set_name(loc);
}

void Texture::load(const TextureLoadInfo& load_info, std::source_location loc) {
  _path = load_info.path;

  uint32_t width, height, chans;
  const uint8_t* data = load_stb_image(_path, &width, &height, &chans);

  if (load_info.preset != Preset::eRTTCube && load_info.preset != Preset::eMapCube) {
    create_texture({width, height, 1}, data, load_info.format, load_info.preset, loc);
  } else {
    auto ia = vuk::ImageAttachment::from_preset(load_info.preset, load_info.format, {width, height, 1}, vuk::Samples::e1);
    ia.usage |= vuk::ImageUsageFlagBits::eTransferDst | vuk::ImageUsageFlagBits::eTransferSrc;
    auto [tex, view, hdr_image] = vuk::create_image_and_view_with_data(*VkContext::get()->superframe_allocator,
                                                                       vuk::DomainFlagBits::eTransferOnTransfer,
                                                                       ia,
                                                                       data);

    auto fut = RendererCommon::generate_cubemap_from_equirectangular(hdr_image);
    vuk::Compiler compiler;
    auto val = fut.get(*VkContext::get()->superframe_allocator, compiler);

    _image = vuk::Unique(*VkContext::get()->superframe_allocator, val->image);
    _view = vuk::Unique(*VkContext::get()->superframe_allocator, val->image_view);
    _attachment.format = val->format;
    _attachment.extent = val->extent;
  }

  delete[] data;
}

void Texture::create_white_texture() {
  OX_SCOPED_ZONE;
  _white_texture = create_shared<Texture>();
  char white_texture_data[16 * 16 * 4];
  memset(white_texture_data, 0xff, 16 * 16 * 4);
  _white_texture->create_texture({16, 16, 1}, white_texture_data, vuk::Format::eR8G8B8A8Unorm, Preset::eRTT2DUnmipped);
}

void Texture::set_name(const std::source_location& loc) {
  const auto ctx = VkContext::get();
  auto file = FileSystem::get_file_name(loc.file_name());
  const auto n = fmt::format("{0}:{1}", file, loc.line());
  ctx->context->set_name(_image->image, vuk::Name(n));
  ctx->context->set_name(_view->payload, vuk::Name(n));
}

uint8_t* Texture::load_stb_image(const std::string& filename, uint32_t* width, uint32_t* height, uint32_t* bits, bool srgb) {
  const auto filePath = std::filesystem::path(filename);

  if (!exists(filePath))
    OX_LOG_ERROR("Couldn't load image, file doesn't exists. {}", filename);

  int tex_width = 0, tex_height = 0, tex_channels = 0;
  constexpr int size_of_channel = 8;

  const auto pixels = stbi_load(filename.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

  if (tex_channels != 4)
    tex_channels = 4;

  if (width)
    *width = tex_width;
  if (height)
    *height = tex_height;
  if (bits)
    *bits = tex_channels * size_of_channel;

  const int32_t size = tex_width * tex_height * tex_channels * size_of_channel / 8;
  auto* result = new uint8_t[size];
  memcpy(result, pixels, size);
  stbi_image_free(pixels);

  return result;
}

uint8_t* Texture::load_stb_image_from_memory(void* buffer, size_t len, uint32_t* width, uint32_t* height, uint32_t* bits, bool flipY, bool srgb) {
  int tex_width = 0, tex_height = 0, tex_channels = 0;
  int size_of_channel = 8;
  const auto pixels = stbi_load_from_memory((stbi_uc*)buffer, (int)len, &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

  if (stbi_is_16_bit_from_memory((stbi_uc*)buffer, (int)len)) {
    size_of_channel = 16;
  }

  if (tex_channels != 4)
    tex_channels = 4;

  if (width)
    *width = tex_width;
  if (height)
    *height = tex_height;
  if (bits)
    *bits = tex_channels * size_of_channel;

  const int32_t size = tex_width * tex_height * tex_channels * size_of_channel / 8;
  auto* result = new uint8_t[size];
  memcpy(result, pixels, size);

  stbi_image_free(pixels);
  return result;
}

uint8_t* Texture::get_magenta_texture(uint32_t width, uint32_t height, uint32_t channels) {
  const uint32_t size = width * height * channels;
  const auto data = new uint8_t[size];

  const uint8_t magenta[16] = {255, 0, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 0, 255, 255};

  memcpy(data, magenta, size);

  return data;
}

uint8_t* Texture::convert_to_four_channels(uint32_t width, uint32_t height, const uint8_t* three_channel_data) {
  const auto bufferSize = width * height * 4;
  const auto buffer = new uint8_t[bufferSize];
  auto* rgba = buffer;
  const auto* rgb = three_channel_data;
  for (uint32_t i = 0; i < width * height; ++i) {
    for (uint32_t j = 0; j < 3; ++j) {
      rgba[j] = rgb[j];
    }
    rgba += 4;
    rgb += 3;
  }
  return buffer;
}
} // namespace ox
