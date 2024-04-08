#pragma once

#include <span>
#include <vector>
#include <vuk/Future.hpp>
#include <vuk/Image.hpp>

namespace vuk {
inline SamplerCreateInfo NearestSamplerClamped = {
  .magFilter = Filter::eNearest,
  .minFilter = Filter::eNearest,
  .mipmapMode = SamplerMipmapMode::eNearest,
  .addressModeU = SamplerAddressMode::eClampToEdge,
  .addressModeV = SamplerAddressMode::eClampToEdge,
  .addressModeW = SamplerAddressMode::eClampToEdge,
};

inline SamplerCreateInfo NearestSamplerRepeated = {
  .magFilter = Filter::eNearest,
  .minFilter = Filter::eNearest,
  .mipmapMode = SamplerMipmapMode::eNearest,
  .addressModeU = SamplerAddressMode::eRepeat,
  .addressModeV = SamplerAddressMode::eRepeat,
  .addressModeW = SamplerAddressMode::eRepeat,
};

inline SamplerCreateInfo NearestMagLinearMinSamplerClamped = {
  .magFilter = Filter::eLinear,
  .minFilter = Filter::eNearest,
  .mipmapMode = SamplerMipmapMode::eNearest,
  .addressModeU = SamplerAddressMode::eClampToEdge,
  .addressModeV = SamplerAddressMode::eClampToEdge,
  .addressModeW = SamplerAddressMode::eClampToEdge,
};

inline SamplerCreateInfo LinearMipmapNearestSamplerClamped = {
  .magFilter = Filter::eNearest,
  .minFilter = Filter::eNearest,
  .mipmapMode = SamplerMipmapMode::eLinear,
  .addressModeU = SamplerAddressMode::eClampToEdge,
  .addressModeV = SamplerAddressMode::eClampToEdge,
  .addressModeW = SamplerAddressMode::eClampToEdge,
};

inline SamplerCreateInfo LinearSamplerRepeated = {
  .magFilter = Filter::eLinear,
  .minFilter = Filter::eLinear,
  .mipmapMode = SamplerMipmapMode::eLinear,
  .addressModeU = SamplerAddressMode::eRepeat,
  .addressModeV = SamplerAddressMode::eRepeat,
  .addressModeW = SamplerAddressMode::eRepeat,
};

inline SamplerCreateInfo LinearSamplerRepeatedAnisotropy = {
  .magFilter = Filter::eLinear,
  .minFilter = Filter::eLinear,
  .mipmapMode = SamplerMipmapMode::eLinear,
  .addressModeU = SamplerAddressMode::eRepeat,
  .addressModeV = SamplerAddressMode::eRepeat,
  .addressModeW = SamplerAddressMode::eRepeat,
  .anisotropyEnable = true,
  .maxAnisotropy = 16.0f,
};

inline SamplerCreateInfo LinearSamplerClamped = {
  .magFilter = Filter::eLinear,
  .minFilter = Filter::eLinear,
  .mipmapMode = SamplerMipmapMode::eLinear,
  .addressModeU = SamplerAddressMode::eClampToEdge,
  .addressModeV = SamplerAddressMode::eClampToEdge,
  .addressModeW = SamplerAddressMode::eClampToEdge,
  .borderColor = BorderColor::eFloatOpaqueWhite,
};

inline SamplerCreateInfo CmpDepthSampler = {
  .magFilter = Filter::eLinear,
  .minFilter = Filter::eLinear,
  .mipmapMode = SamplerMipmapMode::eNearest,
  .addressModeU = SamplerAddressMode::eClampToEdge,
  .addressModeV = SamplerAddressMode::eClampToEdge,
  .addressModeW = SamplerAddressMode::eClampToEdge,
  .compareEnable = true,
  .compareOp = CompareOp::eGreaterOrEqual,
  .minLod = 0.0f,
  .maxLod = 0.0f,
};

inline vuk::ImageAttachment dummy_attachment = {
  .extent = {1, 1, 1},
  .format = vuk::Format::eR8G8B8A8Unorm,
  .sample_count = vuk::SampleCountFlagBits::e1,
  .level_count = 1,
  .layer_count = 1,
};

template <class T>
std::pair<Unique<Buffer>, Value<Buffer>> create_cpu_buffer(Allocator& allocator, std::span<T> data) {
  return create_buffer(allocator, MemoryUsage::eCPUtoGPU, DomainFlagBits::eTransferOnGraphics, data);
}

vuk::Value<vuk::ImageAttachment> generate_mips(vuk::Value<vuk::ImageAttachment> image, uint32_t mip_count);
} // namespace vuk
