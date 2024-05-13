#pragma once
#include <vuk/runtime/CommandBuffer.hpp>

#include "Core/Types.hpp"

namespace ox {
struct Vertex {
  float3 position;
  uint32 normal;
  float2 uv;
};

inline auto vertex_pack = vuk::Packed{
  vuk::Format::eR32G32B32A32Sfloat, // 12 postition
  vuk::Format::eR32G32B32A32Sfloat, // 12 normal
  vuk::Format::eR32G32B32A32Sfloat, // 8  uv
  vuk::Format::eR32G32B32A32Sfloat, // 16 color
  vuk::Format::eR32G32B32A32Sfloat, // 16 joint
  vuk::Format::eR32G32B32A32Sfloat, // 16 weight
};
}
