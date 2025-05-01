#define STB_IMAGE_IMPLEMENTATION

#include "Texture.hpp"

#include <ktx.h>
#include <stb_image.h>
#include <vuk/RenderGraph.hpp>
#include <vuk/runtime/vk/AllocatorHelpers.hpp>
#include <vuk/vsl/Core.hpp>

#include "Core/App.hpp"
#include "Core/FileSystem.hpp"
#include "Render/RendererCommon.hpp"
#include "Render/Utils/VukCommon.hpp"
#include "Render/Vulkan/VkContext.hpp"

namespace ox {
void Texture::create(const std::string& path,
                     const TextureLoadInfo& load_info,
                     const std::source_location& loc) {
  auto& allocator = App::get_vkcontext().superframe_allocator;

  const auto is_generic = load_info.mime == TextureLoadInfo::MimeType::Generic;

  std::unique_ptr<uint8[]> stb_data = nullptr;
  std::unique_ptr<ktxTexture2, decltype([](ktxTexture2* p) { ktxTexture_Destroy(ktxTexture(p)); })> ktx_data = {};

  uint32_t width = {}, height = {}, chans = {};
  vuk::Format format = load_info.format;

  if (is_generic && !path.empty()) {
    stb_data = load_stb_image(path, &width, &height, &chans);
  } else {
    const auto file_data = fs::read_file_binary(path);
    ktxTexture2* ktx{};
    if (const auto result = ktxTexture2_CreateFromMemory(file_data.data(), file_data.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx);
        result != KTX_SUCCESS) {
      OX_LOG_ERROR("Couldn't load KTX2 file {}", ktxErrorString(result));
    }

    auto format_ktx = vuk::Format::eBc7UnormBlock;
    constexpr ktx_transcode_fmt_e ktxTranscodeFormat = KTX_TTF_BC7_RGBA;

    // If the image needs is in a supercompressed encoding, transcode it to a desired format
    if (ktxTexture2_NeedsTranscoding(ktx)) {
      OX_SCOPED_ZONE_N("Transcode KTX 2 Texture");
      if (const auto result = ktxTexture2_TranscodeBasis(ktx, ktxTranscodeFormat, KTX_TF_HIGH_QUALITY); result != KTX_SUCCESS) {
        OX_LOG_ERROR("Couldn't transcode KTX2 file {}", ktxErrorString(result));
      }
    } else {
      // Use the format that the image is already in
      format_ktx = static_cast<vuk::Format>(static_cast<VkFormat>(ktx->vkFormat));
    }

    width = ktx->baseWidth;
    height = ktx->baseHeight;
    format = format_ktx;

    ktx_data.reset(ktx);
  }

  const void* final_data = nullptr;
  if (path.empty()) {
    final_data = load_info.data;
  } else {
    final_data = is_generic ? stb_data.get() : ktx_data.get();
  }

  auto ia = vuk::ImageAttachment::from_preset(load_info.preset, format, {width, height, 1}, vuk::Samples::e1);
  ia.usage |= vuk::ImageUsageFlagBits::eTransferDst | vuk::ImageUsageFlagBits::eTransferSrc;

  auto [tex, view, fut] = vuk::create_image_and_view_with_data(*allocator, vuk::DomainFlagBits::eTransferOnTransfer, ia, final_data);

  if (load_info.preset != Preset::eRTTCube && load_info.preset != Preset::eMapCube) {
    if (ia.level_count > 1)
      fut = vuk::generate_mips(fut, ia.level_count);
  } else {
    fut = RendererCommon::generate_cubemap_from_equirectangular(fut);
  }

  fut.wait(*allocator, _compiler);

  _image = std::move(tex);
  _view = std::move(view);
  _attachment = ia;

  set_name({}, loc);
}

vuk::Value<vuk::ImageAttachment> Texture::acquire(const vuk::Name name,
                                                  const vuk::Access last_access) const {
  return vuk::acquire_ia(name.is_invalid() ? vuk::Name(_name) : name, attachment(), last_access);
}

vuk::Value<vuk::ImageAttachment> Texture::discard(vuk::Name name) const {
  return vuk::discard_ia(name.is_invalid() ? vuk::Name(_name) : name, attachment());
}

void Texture::set_name(std::string_view name,
                       const std::source_location& loc) {
  auto& ctx = App::get_vkcontext();
  if (!name.empty()) {
    ctx.runtime->set_name(_image->image, vuk::Name(name));
    ctx.runtime->set_name(_view->payload, vuk::Name(name));
    _name = name;
  } else {
    auto file = fs::get_file_name(loc.file_name());
    const auto n = fmt::format("{0}:{1}", file, loc.line());
    ctx.runtime->set_name(_image->image, vuk::Name(n));
    ctx.runtime->set_name(_view->payload, vuk::Name(n));
    _name = n;
  }
}

Unique<uint8[]> Texture::load_stb_image(const std::string& filename,
                                        uint32_t* width,
                                        uint32_t* height,
                                        uint32_t* bits,
                                        bool srgb) {
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
  auto result = create_unique<uint8_t[]>(size);
  memcpy(result.get(), pixels, size);
  stbi_image_free(pixels);

  return result;
}

Unique<uint8[]> Texture::load_stb_image_from_memory(void* buffer,
                                                    size_t len,
                                                    uint32_t* width,
                                                    uint32_t* height,
                                                    uint32_t* bits,
                                                    bool flipY,
                                                    bool srgb) {
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
  auto result = create_unique<uint8_t[]>(size);
  memcpy(result.get(), pixels, size);

  stbi_image_free(pixels);
  return result;
}

uint8_t* Texture::get_magenta_texture(uint32_t width,
                                      uint32_t height,
                                      uint32_t channels) {
  const uint32_t size = width * height * channels;
  const auto data = new uint8_t[size];

  const uint8_t magenta[16] = {255, 0, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 0, 255, 255};

  memcpy(data, magenta, size);

  return data;
}

uint8_t* Texture::convert_to_four_channels(uint32_t width,
                                           uint32_t height,
                                           const uint8_t* three_channel_data) {
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
