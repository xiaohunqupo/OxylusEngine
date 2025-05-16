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

struct Sun {
  alignas(4) glm::vec3 direction = {};
  alignas(4) f32 intensity = 10.0f;
};

struct Atmosphere {
  alignas(4) glm::vec3 rayleigh_scatter = {0.005802f,
                                           0.013558f,
                                           0.033100f};
  alignas(4) f32 rayleigh_density = 8.0f;

  alignas(4) glm::vec3 mie_scatter = {0.003996f,
                                      0.003996f,
                                      0.003996f};
  alignas(4) f32 mie_density = 1.2f;
  alignas(4) f32 mie_extinction = 0.004440f;
  alignas(4) f32 mie_asymmetry = 3.6f;

  alignas(4) glm::vec3 ozone_absorption = {0.000650f,
                                           0.001881f,
                                           0.000085f};
  alignas(4) f32 ozone_height = 25.0f;
  alignas(4) f32 ozone_thickness = 15.0f;

  alignas(4) glm::vec3 terrain_albedo = {0.3f,
                                         0.3f,
                                         0.3f};
  alignas(4) f32 aerial_gain_per_slice = 8.0f;
  alignas(4) f32 planet_radius = 6360.0f;
  alignas(4) f32 atmos_radius = 6460.0f;

  alignas(4) vuk::Extent3D transmittance_lut_size = {};
  alignas(4) vuk::Extent3D sky_view_lut_size = {};
  alignas(4) vuk::Extent3D multiscattering_lut_size = {};
  alignas(4) vuk::Extent3D aerial_perspective_lut_size = {};
};

struct CameraData {
  glm::vec4 position = {};

  glm::mat4 projection = {};
  glm::mat4 inv_projection = {};
  glm::mat4 view = {};
  glm::mat4 inv_view = {};
  glm::mat4 projection_view = {};
  glm::mat4 inv_projection_view = {};

  glm::mat4 previous_projection = {};
  glm::mat4 previous_inv_projection = {};
  glm::mat4 previous_view = {};
  glm::mat4 previous_inv_view = {};
  glm::mat4 previous_projection_view = {};
  glm::mat4 previous_inv_projection_view = {};

  glm::vec2 temporalaa_jitter = {};
  glm::vec2 temporalaa_jitter_prev = {};

  glm::vec4 frustum_planes[6] = {};

  glm::vec3 up = {};
  f32 near_clip = 0;
  glm::vec3 forward = {};
  f32 far_clip = 0;
  glm::vec3 right = {};
  f32 fov = 0;
  u32 output_index = 0;
};

constexpr static u32 HISTOGRAM_THREADS_X = 16;
constexpr static u32 HISTOGRAM_THREADS_Y = 16;
constexpr static u32 HISTOGRAM_BIN_COUNT = HISTOGRAM_THREADS_X * HISTOGRAM_THREADS_Y;

struct HistogramLuminance {
  alignas(4) f32 adapted_luminance;
  alignas(4) f32 exposure;
};

struct HistogramInfo {
  alignas(4) f32 min_exposure = -6.0f;
  alignas(4) f32 max_exposure = 18.0f;
  alignas(4) f32 adaptation_speed = 1.1f;
  alignas(4) f32 ev100_bias = 1.0f;
};

} // namespace ox::GPU
