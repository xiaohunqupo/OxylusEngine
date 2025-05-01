#pragma once
#include "Core/UUID.hpp"

namespace ox {
enum class AlphaMode : uint32 {
  Opaque = 0,
  Mask,
  Blend,
};

enum class MaterialID : uint64 { Invalid = std::numeric_limits<uint64>::max() };
struct Material {
  glm::vec4 albedo_color = {1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec3 emissive_color = {0.0f, 0.0f, 0.0f};
  float32 roughness_factor = 0.0f;
  float32 metallic_factor = 0.0f;
  AlphaMode alpha_mode = AlphaMode::Opaque;
  float32 alpha_cutoff = 0.0f;
  UUID albedo_texture = {};
  UUID normal_texture = {};
  UUID emissive_texture = {};
  UUID metallic_roughness_texture = {};
  UUID occlusion_texture = {};
};

enum class MeshID : uint64 { Invalid = std::numeric_limits<uint64>::max() };

class Mesh {
public:
};

} // namespace ox
