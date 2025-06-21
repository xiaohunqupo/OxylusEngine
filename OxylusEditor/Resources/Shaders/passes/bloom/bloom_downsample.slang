module bloom_downsample;

import common;
import gpu;

import passes.bloom.bloom;

[[vk::binding(0, 0)]]
StorageImage2D<f32x4, ImageFormat::RGBA16F> result_image;
[[vk::binding(1, 0)]]
Image2D<f32x4> source_image;
[[vk::binding(2, 0)]]
SamplerState sampler;

[[shader("compute")]]
[[numthreads(8, 8, 1)]]
func cs_main(u32x3 thread_id : SV_DispatchThreadID) {
  u32x2 img_coords = u32x2(thread_id.xy);

  u32 width, height, levels;
  result_image.GetDimensions(0, width, height, levels);
  u32x2 img_size = u32x2(width, height);

  if (any(img_coords >= img_size))
    return;

  f32x2 uv = (img_coords + 0.5) / img_size;

  f32x3 result = downsample(source_image, sampler, uv, 0);
  result_image.Store(img_coords, f32x4(result, 1.0));
}