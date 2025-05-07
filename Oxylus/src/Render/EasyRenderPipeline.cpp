#include "EasyRenderPipeline.hpp"

#include <Core/VFS.hpp>
#include <vuk/vsl/Core.hpp>

#include "Asset/AssetManager.hpp"
#include "Asset/Material.hpp"
#include "Render/Camera.hpp"
#include "Render/DebugRenderer.hpp"
#include "Render/RendererConfig.hpp"
#include "Scene/SceneGPU.hpp"
#include "Slang/Slang.hpp"
#include "Thread/TaskScheduler.hpp"
#include "Utils/VukCommon.hpp"

namespace ox {
void EasyRenderPipeline::init(vuk::Allocator& allocator) {
  auto& runtime = allocator.get_context();

  vuk::PipelineBaseCreateInfo bindless_pci = {};

  auto compile_options = vuk::ShaderCompileOptions{};
  compile_options.compiler_flags = vuk::ShaderCompilerFlagBits::eGlLayout |
                                   vuk::ShaderCompilerFlagBits::eMatrixColumnMajor |
                                   vuk::ShaderCompilerFlagBits::eNoWarnings;
  bindless_pci.set_compile_options(compile_options);

  auto bindless_dslci_00 = vuk::descriptor_set_layout_create_info(
      {
          ds_layout_binding(BindlessID::Scene, vuk::DescriptorType::eStorageBuffer, 1),     // Scene
          ds_layout_binding(BindlessID::Samplers, vuk::DescriptorType::eSampler),           // Samplers
          ds_layout_binding(BindlessID::SampledImages, vuk::DescriptorType::eSampledImage), // SampledImages
      },
      0);
  bindless_pci.explicit_set_layouts.emplace_back(bindless_dslci_00);

  // --- Shaders ---
  auto* vfs = App::get_system<VFS>(EngineSystems::VFS);
  auto shaders_dir = vfs->resolve_physical_dir(VFS::APP_DIR, "Shaders");

  Slang slang = {};
  slang.create_session({.root_directory = shaders_dir, .definitions = {}});

  slang.add_shader(bindless_pci, {.path = shaders_dir + "/2d_forward.slang", .entry_points = {"VSmain", "PSmain"}});
  TRY(allocator.get_context().create_named_pipeline("2d_forward_pipeline", bindless_pci))

  // --- DescriptorSets ---
  this->descriptor_set_00 =
      runtime.create_persistent_descriptorset(allocator, *runtime.get_named_pipeline("2d_forward_pipeline"), 0, 64);

  // --- Samplers ---
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

  const vuk::Sampler linear_sampler_clamped =
      runtime.acquire_sampler(vuk::LinearSamplerClamped, runtime.get_frame_count());
  const vuk::Sampler linear_sampler_repeated =
      runtime.acquire_sampler(vuk::LinearSamplerRepeated, runtime.get_frame_count());
  const vuk::Sampler linear_sampler_repeated_anisotropy =
      runtime.acquire_sampler(vuk::LinearSamplerRepeatedAnisotropy, runtime.get_frame_count());
  const vuk::Sampler nearest_sampler_clamped =
      runtime.acquire_sampler(vuk::NearestSamplerClamped, runtime.get_frame_count());
  const vuk::Sampler nearest_sampler_repeated =
      runtime.acquire_sampler(vuk::NearestSamplerRepeated, runtime.get_frame_count());
  const vuk::Sampler hiz_sampler = runtime.acquire_sampler(hiz_sampler_ci, runtime.get_frame_count());
  this->descriptor_set_00->update_sampler(BindlessID::Samplers, 0, linear_sampler_repeated);
  this->descriptor_set_00->update_sampler(BindlessID::Samplers, 1, linear_sampler_clamped);

  this->descriptor_set_00->update_sampler(BindlessID::Samplers, 2, nearest_sampler_repeated);
  this->descriptor_set_00->update_sampler(BindlessID::Samplers, 3, nearest_sampler_clamped);

  this->descriptor_set_00->update_sampler(BindlessID::Samplers, 4, linear_sampler_repeated_anisotropy);

  this->descriptor_set_00->update_sampler(BindlessID::Samplers, 5, hiz_sampler);

  // const vuk::Sampler cmp_depth_sampler = runtime.acquire_sampler(vuk::CmpDepthSampler, runtime.get_frame_count());
  // this->descriptor_set_00->update_sampler(12, 0, cmp_depth_sampler);
}

void EasyRenderPipeline::shutdown() {}

vuk::Value<vuk::ImageAttachment> EasyRenderPipeline::on_render(vuk::Allocator& frame_allocator,
                                                               const RenderInfo& render_info) {
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

  for (u32 i = 0; i < 6; i++) {
    const auto* plane = Camera::get_frustum(cam, cam.position).planes[i];
    camera_data.frustum_planes[i] = {plane->normal, plane->distance};
  }

  std::vector<CameraData> camera_datas = {};
  camera_datas.emplace_back(camera_data);
  auto [camera_buffer, camera_buffer_fut] = vuk::create_cpu_buffer(frame_allocator, std::span(camera_datas));

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
      .post_processing_data = {},
  };

  auto [scene_buffer, scene_buff_fut] = vuk::create_cpu_buffer(frame_allocator, std::span(&scene_data, 1));
  this->descriptor_set_00->update_storage_buffer(0, 0, *scene_buffer);

  RenderQueue2D render_queue_2d = {};

  render_queue_2d.init();

  std::vector<GPU::Material> materials = {};

  std::vector<vuk::ImageView> texture_views = {};

  for (const auto& sprite_component : this->sprite_component_list) {
    const auto distance =
        glm::distance(glm::vec3(0.f, 0.f, cam.position.z), glm::vec3(0.f, 0.f, sprite_component.get_position().z));

    auto* asset_man = App::get_asset_manager();

    const auto add_texture_if_exists = [&](const UUID& uuid) -> ox::option<u32> {
      if (!uuid) {
        return ox::nullopt;
      }

      auto* texture = asset_man->get_texture(uuid);
      if (!texture) {
        return ox::nullopt;
      }

      auto index = texture_views.size();
      texture_views.emplace_back(*texture->get_view());
      return static_cast<u32>(index);
    };

    if (auto* material = asset_man->get_material(sprite_component.material)) {
      auto gpu_mat =
          GPU::Material::from_material(*material, add_texture_if_exists(material->albedo_texture).value_or(~0_u32));
      gpu_mat.uv_offset = sprite_component.current_uv_offset.value_or(gpu_mat.uv_offset);
      materials.emplace_back(gpu_mat);

      render_queue_2d.add(sprite_component, materials.size() - 1, distance);
    }
  }

  for (u32 i = 0; i < texture_views.size(); i++) {
    this->descriptor_set_00->update_sampled_image(BindlessID::SampledImages,
                                                  i,
                                                  texture_views[i],
                                                  vuk::ImageLayout::eReadOnlyOptimalKHR);
  }

  render_queue_2d.update();
  render_queue_2d.sort();
  this->sprite_component_list.clear();

  if (materials.empty())
    materials.emplace_back();
  auto [material_buffer, material_buffer_fut] = vuk::create_cpu_buffer(frame_allocator, std::span(materials));

  if (render_queue_2d.sprite_data.empty())
    render_queue_2d.sprite_data.emplace_back();
  auto [vertex_buffer_2d, vertex_buffer_2d_fut] =
      vuk::create_cpu_buffer(frame_allocator, std::span(render_queue_2d.sprite_data));

  this->descriptor_set_00->commit(frame_allocator.get_context());

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

  auto color_output_w2d = vuk::make_pass(
      "2d_forward_pass",
      [render_queue_2d, &descriptor_set = *this->descriptor_set_00](vuk::CommandBuffer& command_buffer,
                                                                    VUK_IA(vuk::eColorWrite) target,
                                                                    VUK_IA(vuk::eDepthStencilRW) depth,
                                                                    VUK_BA(vuk::eVertexRead) vertex_buffer,
                                                                    VUK_BA(vuk::eVertexRead) materials,
                                                                    VUK_BA(vuk::eVertexRead) camera) {
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
          .bind_vertex_buffer(0, vertex_buffer, 0, vertex_pack_2d, vuk::VertexInputRate::eInstance)
          .bind_persistent(0, descriptor_set)
          .push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment,
                          0,
                          PushConstants(materials->device_address, camera->device_address))
          .draw(6, batch.count, 0, batch.offset);
    }

    return target;
  })(final_image, depth_image, vertex_buffer_2d_fut, material_buffer_fut, camera_buffer_fut);

  return color_output_w2d;
}

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

  if (static_cast<bool>(RendererCVar::cvar_freeze_culling_frustum.get()) &&
      static_cast<bool>(RendererCVar::cvar_draw_camera_frustum.get())) {
    const auto proj = this->frozen_camera.get_projection_matrix() * this->frozen_camera.get_view_matrix();
    DebugRenderer::draw_frustum(proj, glm::vec4(0, 1, 0, 1), 1.0f, 0.0f); // reversed-z
  }

  this->current_camera = camera;
}

} // namespace ox
