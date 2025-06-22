#pragma once

#include <vuk/Value.hpp>

namespace ox {
class Texture;

class RendererCommon {
public:
  /// Apply gaussian blur in a single pass
  static vuk::Value<vuk::ImageAttachment> apply_blur(const vuk::Value<vuk::ImageAttachment>& src_attachment,
                                                     const vuk::Value<vuk::ImageAttachment>& dst_attachment);

  static vuk::Value<vuk::ImageAttachment>
  generate_cubemap_from_equirectangular(vuk::Value<vuk::ImageAttachment> hdr_image);
};
} // namespace ox
