#pragma once
#include <glm/gtc/packing.hpp>
#include <vuk/runtime/vk/Descriptor.hpp>

#include "RenderPipeline.hpp"
#include "RendererConfig.hpp"

namespace ox {
class EasyRenderPipeline : public RenderPipeline {
public:
  explicit EasyRenderPipeline(const std::string& name) : RenderPipeline(name) {}
  ~EasyRenderPipeline() override = default;

  void init(vuk::Allocator& allocator) override;
  void shutdown() override;

  [[nodiscard]] vuk::Value<vuk::ImageAttachment> on_render(vuk::Allocator& frame_allocator, const RenderInfo& render_info) override;

  void on_update(Scene* scene) override;
  void submit_sprite(const SpriteComponent& sprite) override;
  void submit_camera(const CameraComponent& camera) override;

private:
  // scene textures
  static constexpr auto ALBEDO_IMAGE_INDEX = 0;
  static constexpr auto NORMAL_IMAGE_INDEX = 1;
  static constexpr auto DEPTH_IMAGE_INDEX = 2;
  static constexpr auto SHADOW_ARRAY_INDEX = 3;
  static constexpr auto SKY_TRANSMITTANCE_LUT_INDEX = 4;
  static constexpr auto SKY_MULTISCATTER_LUT_INDEX = 5;
  static constexpr auto VELOCITY_IMAGE_INDEX = 6;
  static constexpr auto BLOOM_IMAGE_INDEX = 7;
  static constexpr auto HIZ_IMAGE_INDEX = 8;
  static constexpr auto VIS_IMAGE_INDEX = 9;
  static constexpr auto METALROUGHAO_IMAGE_INDEX = 10;
  static constexpr auto EMISSION_IMAGE_INDEX = 11;
  static constexpr auto NORMAL_VERTEX_IMAGE_INDEX = 12;

  // buffers and buffer/image combined indices
  static constexpr auto LIGHTS_BUFFER_INDEX = 0;
  static constexpr auto MATERIALS_BUFFER_INDEX = 1;
  static constexpr auto MESH_INSTANCES_BUFFER_INDEX = 2;
  static constexpr auto ENTITIES_BUFFER_INDEX = 3;
  static constexpr auto GTAO_BUFFER_IMAGE_INDEX = 4;
  static constexpr auto TRANSFORMS_BUFFER_INDEX = 5;
  static constexpr auto SPRITE_MATERIALS_BUFFER_INDEX = 6;

  vuk::Unique<vuk::PersistentDescriptorSet> descriptor_set_00 = vuk::Unique<vuk::PersistentDescriptorSet>();

  std::vector<SpriteComponent> sprite_component_list = {};

  CameraComponent current_camera = {};
  CameraComponent frozen_camera = {};
  bool saved_camera = false;

  struct DrawBatch2D {
    vuk::Name pipeline_name = {};
    uint32 offset = 0;
    uint32 count = 0;
  };

  enum RenderFlags2D : uint32 {
    RENDER_FLAGS_2D_NONE = 0,

    RENDER_FLAGS_2D_SORT_Y = 1 << 0,
    RENDER_FLAGS_2D_FLIP_X = 1 << 1,
  };

  struct SpriteGPUData {
    glm::mat4 transform = {};
    uint32 material_id16_ypos16 = 0;
    uint32 flags16_distance16 = 0;

    bool operator>(const SpriteGPUData& other) const {
      union SortKey {
        struct {
          // The order of members is important here, it means the sort priority (low to high)!
          uint64_t distance_y : 32;
          uint64_t distance_z : 32;
        } bits;

        uint64_t value;
      };
      static_assert(sizeof(SortKey) == sizeof(uint64_t));
      const SortKey a = {
        .bits =
          {
            .distance_y = math::unpack_u32_low(flags16_distance16) & RENDER_FLAGS_2D_SORT_Y ? math::unpack_u32_high(material_id16_ypos16) : 0u,
            .distance_z = math::unpack_u32_high(flags16_distance16),
          },
      };
      const SortKey b = {
        .bits =
          {
            .distance_y = math::unpack_u32_low(other.flags16_distance16) & RENDER_FLAGS_2D_SORT_Y ? math::unpack_u32_high(other.material_id16_ypos16)
                                                                                                  : 0u,
            .distance_z = math::unpack_u32_high(other.flags16_distance16),
          },
      };
      return a.value > b.value;
    }
  };

  struct RenderQueue2D {
    std::vector<DrawBatch2D> batches = {};
    std::vector<SpriteGPUData> sprite_data = {};

    vuk::Name current_pipeline_name = {};

    uint32 num_sprites = 0;
    uint32 previous_offset = 0;

    uint32 last_batches_size = 0;
    uint32 last_sprite_data_size = 0;
    uint32 last_material_id = 0;

    void init() {
      batches.reserve(last_batches_size);
      sprite_data.reserve(last_sprite_data_size);
    }

    // TODO: this will take a list of materials
    // TODO: sort pipelines
    void update() {
      const vuk::Name pipeline_name = "2d_forward_pipeline"; // FIXME: hardcoded until we have a modular material shader system
      if (current_pipeline_name != pipeline_name) {
        batches.emplace_back(DrawBatch2D{.pipeline_name = pipeline_name, .offset = previous_offset, .count = num_sprites - previous_offset});
        current_pipeline_name = pipeline_name;
      }

      previous_offset = num_sprites;
    }

    void add(const SpriteComponent& sprite, const float distance) {
      sprite.material->set_id(last_material_id);
      last_material_id += 1;

      uint16 flags = 0;
      if (sprite.sort_y)
        flags |= RENDER_FLAGS_2D_SORT_Y;

      if (sprite.flip_x)
        flags |= RENDER_FLAGS_2D_FLIP_X;

      const uint32 flags_and_distance = math::pack_u16(flags, glm::packHalf1x16(distance));
      const uint32 materialid_and_ypos = math::pack_u16(static_cast<uint16>(sprite.material->get_id()), glm::packHalf1x16(sprite.get_position().y));

      sprite_data.emplace_back(SpriteGPUData{
        .transform = sprite.transform,
        .material_id16_ypos16 = materialid_and_ypos,
        .flags16_distance16 = flags_and_distance,
      });

      num_sprites += 1;
    }

    void sort() { std::ranges::sort(sprite_data, std::greater<SpriteGPUData>()); }

    void clear() {
      num_sprites = 0;
      previous_offset = 0;
      last_material_id = 0;
      last_batches_size = static_cast<uint32>(batches.size());
      last_sprite_data_size = static_cast<uint32>(sprite_data.size());
      current_pipeline_name = {};

      batches.clear();
      sprite_data.clear();
    }
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
    float near_clip = 0;
    glm::vec3 forward = {};
    float far_clip = 0;
    glm::vec3 right = {};
    float fov = 0;
    glm::vec3 _pad = {};
    uint32_t output_index = 0;
  };

  struct CameraConstantBuffer {
    CameraData camera_data[16] = {};
  };

  struct SceneData {
    uint32 num_lights = {};
    float32 grid_max_distance = {};
    glm::uvec2 screen_size = {};
    int32 draw_meshlet_aabbs = {};

    glm::vec2 screen_size_rcp = {};
    glm::uvec2 shadow_atlas_res = {};

    glm::vec3 sun_direction = {};
    uint32 meshlet_count = {};

    glm::vec4 sun_color = {}; // pre-multipled with intensity

    static constexpr int32 INVALID_INDEX = -1;
    struct Indices {
      int32 albedo_image_index = INVALID_INDEX;
      int32 normal_image_index = INVALID_INDEX;
      int32 normal_vertex_image_index = INVALID_INDEX;
      int32 depth_image_index = INVALID_INDEX;
      int32 bloom_image_index = INVALID_INDEX;
      int32 mesh_instance_buffer_index = INVALID_INDEX;
      int32 entites_buffer_index = INVALID_INDEX;
      int32 materials_buffer_index = INVALID_INDEX;
      int32 lights_buffer_index = INVALID_INDEX;
      int32 sky_env_map_index = INVALID_INDEX;
      int32 sky_transmittance_lut_index = INVALID_INDEX;
      int32 sky_multiscatter_lut_index = INVALID_INDEX;
      int32 velocity_image_index = INVALID_INDEX;
      int32 shadow_array_index = INVALID_INDEX;
      int32 gtao_buffer_image_index = INVALID_INDEX;
      int32 hiz_image_index = INVALID_INDEX;
      int32 vis_image_index = INVALID_INDEX;
      int32 emission_image_index = INVALID_INDEX;
      int32 metallic_roughness_ao_image_index = INVALID_INDEX;
      int32 transforms_buffer_index = INVALID_INDEX;
      int32 sprite_materials_buffer_index = INVALID_INDEX;
    } indices = {};

    struct PostProcessingData {
      int32 tonemapper = RendererConfig::TONEMAP_ACES;
      float32 exposure = 1.0f;
      float32 gamma = 2.5f;

      int32 enable_bloom = 1;
      int32 enable_ssr = 1;
      int32 enable_gtao = 1;

      glm::vec4 vignette_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.25f); // rgb: color, a: intensity
      glm::vec4 vignette_offset = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // xy: offset, z: useMask, w: enable effect
      glm::vec2 film_grain = {};                                     // x: enable, y: amount
      glm::vec2 chromatic_aberration = {};                           // x: enable, y: amount
      glm::vec2 sharpen = {};                                        // x: enable, y: amount
    } post_processing_data = {};
  };
};
} // namespace ox
