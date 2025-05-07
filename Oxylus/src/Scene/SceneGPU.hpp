#pragma once

#include "Asset/Material.hpp"
namespace ox::GPU {

struct Material {
  alignas(4) glm::vec4 albedo_color = glm::vec4(1.0f);
  alignas(4) glm::vec2 uv_size = glm::vec3(1.0f);
  alignas(4) glm::vec2 uv_offset = glm::vec3(0.0f);
  alignas(4) glm::vec3 emissive_color = glm::vec3(1.0f);
  alignas(4) f32 roughness_factor = 0.0f;
  alignas(4) f32 metallic_factor = 0.0f;
  alignas(4) AlphaMode alpha_mode = AlphaMode::Opaque;
  alignas(4) f32 alpha_cutoff = 0.0f;
  alignas(4) SamplingMode sampling_mode = SamplingMode::LinearRepeated;
  alignas(4) u32 albedo_image_index = ~0_u32;
  alignas(4) u32 normal_image_index = ~0_u32;
  alignas(4) u32 emissive_image_index = ~0_u32;
  alignas(4) u32 metallic_roughness_image_index = ~0_u32;
  alignas(4) u32 occlusion_image_index = ~0_u32;

  static GPU::Material from_material(const ox::Material& material,
                                     u32 albedo_id = ~0_u32,
                                     u32 normal_id = ~0_u32,
                                     u32 emissive_id = ~0_u32,
                                     u32 metallic_roughness_id = ~0_u32,
                                     u32 occlusion_id = ~0_u32) {
    return {
        .albedo_color = material.albedo_color,
        .uv_size = material.uv_size,
        .uv_offset = material.uv_offset,
        .emissive_color = material.emissive_color,
        .roughness_factor = material.roughness_factor,
        .metallic_factor = material.metallic_factor,
        .alpha_mode = material.alpha_mode,
        .alpha_cutoff = material.alpha_cutoff,
        .sampling_mode = material.sampling_mode,
        .albedo_image_index = albedo_id,
        .normal_image_index = normal_id,
        .emissive_image_index = emissive_id,
        .metallic_roughness_image_index = metallic_roughness_id,
        .occlusion_image_index = occlusion_id,
    };
  };
};
} // namespace ox::GPU
