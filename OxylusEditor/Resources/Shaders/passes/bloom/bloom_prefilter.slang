module bloom_prefilter;

import common;
import gpu;
import passes.bloom.bloom;

[[vk::binding(0, 0)]]
StorageImage2D<f32x4, ImageFormat::RGBA16F> result_image;
[[vk::binding(1, 0)]]
Image2D<f32x4> source_image;
[[vk::binding(2, 0)]]
SamplerState sampler;

struct PushConstants {
  f32 threshold;
  f32 clamp;
};
[[vk::push_constant]] PushConstants C;

[[shader("compute")]]
[[numthreads(8, 8, 1)]]
func cs_main(u32x3 thread_id : SV_DispatchThreadID) {
  u32x2 img_coords = thread_id.xy;

  u32 width, height, levels;
  result_image.GetDimensions(0, width, height, levels);
  u32x2 img_size = u32x2(width, height);

  if (any(img_coords >= img_size))
    return;

  f32x2 uv = (img_coords + 0.5f) / img_size;

  const f32 epsilon = 0.0001f;

  f32x3 result = downsample(source_image, sampler, uv, 0);
  result = prefilter(result, C.clamp, C.threshold, epsilon);
  result_image.store(img_coords, f32x4(result, 1.0f));
}