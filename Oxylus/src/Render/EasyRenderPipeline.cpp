#include "EasyRenderPipeline.hpp"

#include <Core/VFS.hpp>
#include <vuk/vsl/Core.hpp>

#include "Camera.hpp"
#include "Core/FileSystem.hpp"
#include "DebugRenderer.hpp"
#include "DefaultRenderPipeline.hpp"
#include "RendererConfig.hpp"
#include "Slang/Slang.hpp"
#include "Thread/TaskScheduler.hpp"
#include "Utils/VukCommon.hpp"

namespace ox {
void EasyRenderPipeline::init(vuk::Allocator& allocator) {
  auto& runtime = allocator.get_context();

  vuk::PipelineBaseCreateInfo bindless_pci = {};

  auto compile_options = vuk::ShaderCompileOptions{};
  compile_options.compiler_flags = vuk::ShaderCompilerFlagBits::eGlLayout | vuk::ShaderCompilerFlagBits::eMatrixColumnMajor |
                                   vuk::ShaderCompilerFlagBits::eNoWarnings;
  bindless_pci.set_compile_options(compile_options);

  auto bindless_dslci_00 = vuk::descriptor_set_layout_create_info(
    {
      ds_layout_binding(0, vuk::DescriptorType::eStorageBuffer, 1),
      ds_layout_binding(1, vuk::DescriptorType::eStorageBuffer),
      ds_layout_binding(2, vuk::DescriptorType::eStorageBuffer),
      ds_layout_binding(3, vuk::DescriptorType::eSampledImage),
      ds_layout_binding(4, vuk::DescriptorType::eSampledImage),
      ds_layout_binding(5, vuk::DescriptorType::eSampledImage),
      ds_layout_binding(6, vuk::DescriptorType::eSampledImage, 8),
      ds_layout_binding(7, vuk::DescriptorType::eSampledImage, 8),
      ds_layout_binding(8, vuk::DescriptorType::eStorageImage),
      ds_layout_binding(9, vuk::DescriptorType::eStorageImage),
      ds_layout_binding(10, vuk::DescriptorType::eSampledImage),
      ds_layout_binding(11, vuk::DescriptorType::eSampler),
      ds_layout_binding(12, vuk::DescriptorType::eSampler),
    },
    0);
  bindless_pci.explicit_set_layouts.emplace_back(bindless_dslci_00);

  auto* task_scheduler = App::get_system<TaskScheduler>(EngineSystems::TaskScheduler);

  const auto shader_path = [](const std::string& path) -> std::string {
    auto* vfs = App::get_system<VFS>(EngineSystems::VFS);
    return vfs->resolve_physical_dir(VFS::APP_DIR, path);
  };

  task_scheduler->add_task([=]() mutable {
    Slang::add_shader(bindless_pci, {.path = shader_path("Shaders/2DForward.slang"), .entry_points = {"VSmain", "PSmain"}, .definitions = {}});
    TRY(allocator.get_context().create_named_pipeline("2d_forward_pipeline", bindless_pci))
  });

  task_scheduler->wait_for_all();

  auto hiz_sampler_ci = vuk::SamplerCreateInfo{
    .magFilter = vuk::Filter::eNearest,
    .minFilter = vuk::Filter::eNearest,
    .mipmapMode = vuk::SamplerMipmapMode::eNearest,
    .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
    .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
    .addressModeW = vuk::SamplerAddressMode::eClampToEdge,
    .minLod = 0.0f,
    .maxLod = 16.0f,
  };

  this->descriptor_set_00 = runtime.create_persistent_descriptorset(allocator, *runtime.get_named_pipeline("2d_forward_pipeline"), 0, 64);
  const vuk::Sampler linear_sampler_clamped = runtime.acquire_sampler(vuk::LinearSamplerClamped, runtime.get_frame_count());
  const vuk::Sampler linear_sampler_repeated = runtime.acquire_sampler(vuk::LinearSamplerRepeated, runtime.get_frame_count());
  const vuk::Sampler linear_sampler_repeated_anisotropy = runtime.acquire_sampler(vuk::LinearSamplerRepeatedAnisotropy, runtime.get_frame_count());
  const vuk::Sampler nearest_sampler_clamped = runtime.acquire_sampler(vuk::NearestSamplerClamped, runtime.get_frame_count());
  const vuk::Sampler nearest_sampler_repeated = runtime.acquire_sampler(vuk::NearestSamplerRepeated, runtime.get_frame_count());
  const vuk::Sampler cmp_depth_sampler = runtime.acquire_sampler(vuk::CmpDepthSampler, runtime.get_frame_count());
  const vuk::Sampler hiz_sampler = runtime.acquire_sampler(hiz_sampler_ci, runtime.get_frame_count());
  this->descriptor_set_00->update_sampler(11, 0, linear_sampler_clamped);
  this->descriptor_set_00->update_sampler(11, 1, linear_sampler_repeated);
  this->descriptor_set_00->update_sampler(11, 2, linear_sampler_repeated_anisotropy);
  this->descriptor_set_00->update_sampler(11, 3, nearest_sampler_clamped);
  this->descriptor_set_00->update_sampler(11, 4, nearest_sampler_repeated);
  this->descriptor_set_00->update_sampler(11, 5, hiz_sampler);
  this->descriptor_set_00->update_sampler(12, 0, cmp_depth_sampler);
}

void EasyRenderPipeline::shutdown() {}

vuk::Value<vuk::ImageAttachment> EasyRenderPipeline::on_render(vuk::Allocator& frame_allocator, const RenderInfo& render_info) {
  OX_SCOPED_ZONE;

  const bool freeze_culling = static_cast<bool>(RendererCVar::cvar_freeze_culling_frustum.get());
  CameraComponent cam = freeze_culling ? frozen_camera : current_camera;

  auto camera_data = CameraData{
    .position = glm::vec4(cam.position, 0.0f),
    .projection = cam.get_projection_matrix(),
    .inv_projection = cam.get_inv_projection_matrix(),
    .view = cam.get_view_matrix(),
    .inv_view = cam.get_inv_view_matrix(),
    .projection_view = cam.get_projection_matrix() * cam.get_view_matrix(),
    .inv_projection_view = cam.get_inverse_projection_view(),
    .previous_projection = cam.get_projection_matrix(),
    .previous_inv_projection = cam.get_inv_projection_matrix(),
    .previous_view = cam.get_view_matrix(),
    .previous_inv_view = cam.get_inv_view_matrix(),
    .previous_projection_view = cam.get_projection_matrix() * cam.get_view_matrix(),
    .previous_inv_projection_view = cam.get_inverse_projection_view(),
    .temporalaa_jitter = cam.jitter,
    .temporalaa_jitter_prev = cam.jitter_prev,
    .near_clip = cam.near_clip,
    .far_clip = cam.far_clip,
    .fov = cam.fov,
    .output_index = 0,
  };

  for (uint32 i = 0; i < 6; i++) {
    const auto* plane = Camera::get_frustum(cam, cam.position).planes[i];
    camera_data.frustum_planes[i] = {plane->normal, plane->distance};
  }

  const SceneData scene_data = {
    .num_lights = {},
    .grid_max_distance = RendererCVar::cvar_draw_grid_distance.get(),
    .screen_size = {render_info.extent.width, render_info.extent.height},
    .draw_meshlet_aabbs = RendererCVar::cvar_draw_meshlet_aabbs.get(),
    .screen_size_rcp = {1.0f / static_cast<float>(std::max(1u, scene_data.screen_size.x)),
                        1.0f / static_cast<float>(std::max(1u, scene_data.screen_size.y))},
    .shadow_atlas_res = {},
    .sun_direction = {},
    .meshlet_count = {},
    .sun_color = {},
    .indices =
      {
        .albedo_image_index = ALBEDO_IMAGE_INDEX,
        .normal_image_index = NORMAL_IMAGE_INDEX,
        .normal_vertex_image_index = NORMAL_VERTEX_IMAGE_INDEX,
        .depth_image_index = DEPTH_IMAGE_INDEX,
        .bloom_image_index = BLOOM_IMAGE_INDEX,
        .mesh_instance_buffer_index = MESH_INSTANCES_BUFFER_INDEX,
        .entites_buffer_index = ENTITIES_BUFFER_INDEX,
        .materials_buffer_index = MATERIALS_BUFFER_INDEX,
        .lights_buffer_index = LIGHTS_BUFFER_INDEX,
        .sky_env_map_index = {},
        .sky_transmittance_lut_index = SKY_TRANSMITTANCE_LUT_INDEX,
        .sky_multiscatter_lut_index = SKY_MULTISCATTER_LUT_INDEX,
        .velocity_image_index = VELOCITY_IMAGE_INDEX,
        .shadow_array_index = SHADOW_ARRAY_INDEX,
        .gtao_buffer_image_index = GTAO_BUFFER_IMAGE_INDEX,
        .hiz_image_index = HIZ_IMAGE_INDEX,
        .vis_image_index = VIS_IMAGE_INDEX,
        .emission_image_index = EMISSION_IMAGE_INDEX,
        .metallic_roughness_ao_image_index = METALROUGHAO_IMAGE_INDEX,
        .transforms_buffer_index = TRANSFORMS_BUFFER_INDEX,
        .sprite_materials_buffer_index = SPRITE_MATERIALS_BUFFER_INDEX,
      },
    .post_processing_data = {},
  };

  auto [scene_buffer, scene_buff_fut] = vuk::create_cpu_buffer(frame_allocator, std::span(&scene_data, 1));
  this->descriptor_set_00->update_storage_buffer(0, 0, *scene_buffer);

  RenderQueue2D render_queue_2d = {};

  render_queue_2d.init();

  std::vector<SpriteMaterial::Parameters> sprite_material_parameters = {};
  sprite_material_parameters.reserve(this->sprite_component_list.size());

  for (const auto& sprite_component : this->sprite_component_list) {
    const auto distance = glm::distance(glm::vec3(0.f, 0.f, cam.position.z), glm::vec3(0.f, 0.f, sprite_component.get_position().z));
    render_queue_2d.add(sprite_component, distance);

    const auto& material = sprite_component.material;
    const auto& albedo = material->get_albedo_texture();

    if (albedo && albedo->is_valid_id())
      this->descriptor_set_00->update_sampled_image(10, albedo->get_id(), *albedo->get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);

    SpriteMaterial::Parameters par = material->parameters;
    par.uv_offset = sprite_component.current_uv_offset.value_or(material->parameters.uv_offset);
    sprite_material_parameters.emplace_back(par);
  }

  render_queue_2d.update();
  render_queue_2d.sort();
  this->sprite_component_list.clear();

  if (sprite_material_parameters.empty())
    sprite_material_parameters.emplace_back();
  auto [sprite_mat_buffer, sprite_mat_buff_fut] = vuk::create_cpu_buffer(frame_allocator, std::span(sprite_material_parameters));
  this->descriptor_set_00->update_storage_buffer(1, SPRITE_MATERIALS_BUFFER_INDEX, *sprite_mat_buffer);

  this->descriptor_set_00->commit(frame_allocator.get_context());

  const auto vertex_buffer_2d = *vuk::allocate_cpu_buffer(frame_allocator, sizeof(SpriteGPUData) * 3000);
  std::memcpy(vertex_buffer_2d.mapped_ptr, render_queue_2d.sprite_data.data(), sizeof(SpriteGPUData) * render_queue_2d.sprite_data.size());

  const auto final_ia = vuk::ImageAttachment{
    .extent = render_info.extent,
    .format = render_info.format,
    .sample_count = vuk::SampleCountFlagBits::e1,
    .level_count = 1,
    .layer_count = 1,
  };
  auto final_image = vuk::clear_image(vuk::declare_ia("final_image", final_ia), vuk::Black<float>);

  const auto depth_ia = vuk::ImageAttachment{
    .extent = render_info.extent,
    .format = vuk::Format::eD32Sfloat,
    .sample_count = vuk::SampleCountFlagBits::e1,
    .level_count = 1,
    .layer_count = 1,
  };
  auto depth_image = vuk::clear_image(vuk::declare_ia("depth_image", depth_ia), vuk::DepthZero);

  auto color_output_w2d = vuk::make_pass("2d_forward_pass",
                                         [camera_data,
                                          render_queue_2d,
                                          &descriptor_set = *this->descriptor_set_00,
                                          vertex_buffer_2d](vuk::CommandBuffer& command_buffer,
                                                            VUK_IA(vuk::eColorWrite) target,
                                                            VUK_IA(vuk::eDepthStencilRW) depth) {
    const auto vertex_pack_2d = vuk::Packed{
      vuk::Format::eR32G32B32A32Sfloat, // 16 row
      vuk::Format::eR32G32B32A32Sfloat, // 16 row
      vuk::Format::eR32G32B32A32Sfloat, // 16 row
      vuk::Format::eR32G32B32A32Sfloat, // 16 row
      vuk::Format::eR32Uint,            // 4 material_id
      vuk::Format::eR32Uint,            // 4 flags
    };

    for (const auto& batch : render_queue_2d.batches) {
      if (batch.count < 1)
        continue;

      command_buffer.bind_graphics_pipeline(batch.pipeline_name)
        .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
          .depthTestEnable = true,
          .depthWriteEnable = false,
          .depthCompareOp = vuk::CompareOp::eGreaterOrEqual,
        })
        .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
        .set_viewport(0, vuk::Rect2D::framebuffer())
        .set_scissor(0, vuk::Rect2D::framebuffer())
        .broadcast_color_blend(vuk::BlendPreset::eAlphaBlend)
        .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
        .bind_vertex_buffer(0, vertex_buffer_2d, 0, vertex_pack_2d, vuk::VertexInputRate::eInstance)
        .bind_persistent(0, descriptor_set);

      CameraConstantBuffer camera_constant_buffer = {};
      camera_constant_buffer.camera_data[0] = camera_data;
      const auto cb = command_buffer.scratch_buffer<CameraConstantBuffer>(1, 0);
      *cb = camera_constant_buffer;

      command_buffer.draw(6, batch.count, 0, batch.offset);
    }

    return target;
  })(final_image, depth_image);

  return color_output_w2d;
}

void EasyRenderPipeline::on_update(Scene* scene) {}

void EasyRenderPipeline::submit_sprite(const SpriteComponent& sprite) {
  OX_SCOPED_ZONE;
  this->sprite_component_list.emplace_back(sprite);
}

void EasyRenderPipeline::submit_camera(const CameraComponent& camera) {
  OX_SCOPED_ZONE;

  const auto freeze_culling = static_cast<bool>(RendererCVar::cvar_freeze_culling_frustum.get());
  if (freeze_culling && !this->saved_camera) {
    this->saved_camera = true;
    frozen_camera = current_camera;
  } else if (!freeze_culling && this->saved_camera) {
    this->saved_camera = false;
  }

  if (static_cast<bool>(RendererCVar::cvar_freeze_culling_frustum.get()) && static_cast<bool>(RendererCVar::cvar_draw_camera_frustum.get())) {
    const auto proj = this->frozen_camera.get_projection_matrix() * this->frozen_camera.get_view_matrix();
    DebugRenderer::draw_frustum(proj, glm::vec4(0, 1, 0, 1), 1.0f, 0.0f); // reversed-z
  }

  this->current_camera = camera;
}

} // namespace ox
