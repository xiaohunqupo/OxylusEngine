module material;

import common;
import gpu;

public const static f32 CAMERA_SCALE_UNIT     = 0.01;
public const static f32 INV_CAMERA_SCALE_UNIT = 1.0 / CAMERA_SCALE_UNIT;
public const static f32 PLANET_RADIUS_OFFSET  = 0.001;

public enum AlphaMode : u32 {
    Opaque = 0,
    Mask,
    Blend,
};

public struct Material {
  public f32x4 albedo_color = {};
  public f32x2 uv_size = {};
  public f32x2 uv_offset = {};
  public f32x3 emissive_color = {};
  public f32 roughness_factor = 0.0;
  public f32 metallic_factor = 0.0;
  public AlphaMode alpha_mode = AlphaMode::Opaque;
  public f32 alpha_cutoff = 0.0;
  public u32 sampling_mode = 0;
  public u32 albedo_image_index = ~0u;
  public u32 normal_image_index = ~0u;
  public u32 emissive_image_index = ~0u;
  public u32 metallic_rougness_image_index = ~0u;
  public u32 occlusion_image_index = ~0u;

  public func sample_albedo_color(f32x2 uv) -> f32x4 { 
    if (this.albedo_image_index != ~0u) {
      const let color = material_images[this.albedo_image_index]
        .sample(material_sampler[this.sampling_mode], uv);
      return color * this.albedo_color;
    }

    return this.albedo_color;
  }

  public func samplegrad_albedo_color(f32x2 uv, f32x2 ddx, f32x2 ddy) -> f32x4 {
    if (this.albedo_image_index != ~0u) {
      const let color = material_images[this.albedo_image_index]
        .sample_grad(material_sampler[this.sampling_mode], uv, ddx, ddy);
      return this.albedo_color * color;
    }
    return this.albedo_color;
  }

  public func samplegrad_normal_color(f32x2 uv, f32x2 ddx, f32x2 ddy) -> f32x3 {
    if (this.normal_image_index != ~0u) {
      return material_images[this.normal_image_index]
            .sample_grad(material_sampler[this.sampling_mode], uv, ddx, ddy).rgb;
    }

    return { 0.0 };
  }

  public func samplegrad_emissive_color(f32x2 uv, f32x2 ddx, f32x2 ddy) -> f32x3 {
    if (this.emissive_image_index != ~0u) {
        const let color = material_images[this.emissive_image_index]
            .sample_grad(material_sampler[this.sampling_mode], uv, ddx, ddy).rgb;
        return this.emissive_color * color;
    }
    return this.emissive_color;
  }

  public func samplegrad_metallic_roughness(f32x2 uv, f32x2 ddx, f32x2 ddy) -> f32x2 {
    const let metallic_roughness = f32x2(this.metallic_factor, this.roughness_factor);
    if (this.metallic_rougness_image_index != ~0u) {
        const let color = material_images[this.metallic_rougness_image_index]
            .sample_grad(material_sampler[this.sampling_mode], uv, ddx, ddy).bg;
        return metallic_roughness * color;
    }
    return metallic_roughness;
  }

  public func samplegrad_occlusion_color(f32x2 uv, f32x2 ddx, f32x2 ddy) -> f32 {
    if (this.occlusion_image_index != ~0u) {
        return material_images[this.occlusion_image_index]
            .sample_grad(material_sampler[this.sampling_mode], uv, ddx, ddy).r;
    }
    return 1.0;
  }
};

public struct Camera {
  public f32x4 position = {};
  public mat4 projection = {};
  public mat4 inv_projection = {};
  public mat4 view = {};
  public mat4 inv_view = {};
  public mat4 projection_view = {};
  public mat4 inv_projection_view = {};
  public mat4 previous_projection = {};
  public mat4 previous_inv_projection = {};
  public mat4 previous_view = {};
  public mat4 previous_inv_view = {};
  public mat4 previous_projection_view = {};
  public mat4 previous_inv_projection_view = {};
  public f32x2 temporalaa_jitter = {};
  public f32x2 temporalaa_jitter_prev = {};
  public f32x4 frustum_planes[6] = {};
  public f32x3 up = {};
  public f32 near_clip = 0;
  public f32x3 forward = {};
  public f32 far_clip = 0;
  public f32x3 right = {};
  public f32 fov = 0;
  public u32 output_index = 0;
};

public struct Sun {
    public f32x3 direction;
    public f32   intensity;
};

public struct Atmosphere {
    public f32x3 eye_pos;

    public f32x3 rayleigh_scatter;
    public f32   rayleigh_density;

    public f32x3 mie_scatter;
    public f32   mie_density;
    public f32   mie_extinction;
    public f32   mie_asymmetry;

    public f32x3 ozone_absorption;
    public f32   ozone_height;
    public f32   ozone_thickness;

    public f32x3 terrain_albedo;
    public f32   planet_radius;
    public f32   atmos_radius;
    public f32   aerial_perspective_start_km;

    public i32x3 transmittance_lut_size;
    public i32x3 sky_view_lut_size;
    public i32x3 multiscattering_lut_size;
    public i32x3 aerial_perspective_lut_size;
};

// --- Bindings ---

[[vk::binding(0, 0)]]
public Sampler material_sampler[];
[[vk::binding(1, 0)]]
public Image2D<f32x4> material_images[];
