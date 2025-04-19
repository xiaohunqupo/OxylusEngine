#include "EasyRenderPipeline.hpp"

#include <vuk/vsl/Core.hpp>

#include "Camera.hpp"
#include "Core/FileSystem.hpp"
#include "DebugRenderer.hpp"
#include "RendererConfig.hpp"
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

  auto* task_scheduler = App::get_system<TaskScheduler>();

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(fs::read_shader_file("2DForward.hlsl"), fs::get_shader_path("2DForward.hlsl"), vuk::HlslShaderStage::eVertex, "VSmain");
    bindless_pci.add_hlsl(fs::read_shader_file("2DForward.hlsl"), fs::get_shader_path("2DForward.hlsl"), vuk::HlslShaderStage::ePixel, "PSmain");
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

vuk::Value<vuk::ImageAttachment> EasyRenderPipeline::on_render(vuk::Allocator& frame_allocator, const vuk::Extent3D ext, const vuk::Format format) {
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
      descriptor_set_00->update_sampled_image(10, albedo->get_id(), *albedo->get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);

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

  const auto vertex_buffer_2d = *vuk::allocate_cpu_buffer(frame_allocator, sizeof(SpriteGPUData) * 3000);
  std::memcpy(vertex_buffer_2d.mapped_ptr, render_queue_2d.sprite_data.data(), sizeof(SpriteGPUData) * render_queue_2d.sprite_data.size());

  const auto final_ia = vuk::ImageAttachment{
    .extent = ext,
    .format = format,
    .sample_count = vuk::SampleCountFlagBits::e1,
    .level_count = 1,
    .layer_count = 1,
  };
  auto final_image = vuk::clear_image(vuk::declare_ia("final_image", final_ia), vuk::Black<float>);

  auto color_output_w2d = vuk::make_pass("2d_forward_pass",
                                         [camera_data,
                                          render_queue_2d,
                                          &descriptor_set = *this->descriptor_set_00,
                                          vertex_buffer_2d](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eColorWrite) target
                                                            /*VUK_IA(vuk::eDepthStencilRW) depth*/) {
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

      command_buffer
        .bind_graphics_pipeline(batch.pipeline_name)
        //.set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
        //  .depthTestEnable = true,
        //  .depthWriteEnable = false,
        //  .depthCompareOp = vuk::CompareOp::eGreaterOrEqual,
        //})
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
  })(final_image /*, depth_output*/);

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
