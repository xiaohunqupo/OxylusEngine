#include "EasyRenderPipeline.hpp"

#include <flecs/addons/cpp/entity.hpp>
#include <vuk/ImageAttachment.hpp>
#include <vuk/RenderGraph.hpp>
#include <vuk/Types.hpp>
#include <vuk/Value.hpp>
#include <vuk/runtime/vk/Allocator.hpp>
#include <vuk/runtime/vk/Image.hpp>
#include <vuk/vsl/Core.hpp>

#include "Asset/AssetManager.hpp"
#include "Asset/Texture.hpp"
#include "Core/VFS.hpp"
#include "Render/Camera.hpp"
#include "Render/DebugRenderer.hpp"
#include "Render/RendererConfig.hpp"
#include "Render/Slang/Slang.hpp"
#include "Render/Utils/VukCommon.hpp"
#include "Render/Vulkan/VkContext.hpp"
#include "Scene/ECSModule/Core.hpp"
#include "Scene/SceneGPU.hpp"
#include "Utils/Profiler.hpp"

namespace ox {
auto EasyRenderPipeline::init(VkContext& vk_context) -> void {
  auto& runtime = *vk_context.runtime;
  auto& allocator = *vk_context.superframe_allocator;

  auto dslci_00 = vuk::descriptor_set_layout_create_info(
      {
          ds_layout_binding(BindlessID::Samplers, vuk::DescriptorType::eSampler),           // Samplers
          ds_layout_binding(BindlessID::SampledImages, vuk::DescriptorType::eSampledImage), // SampledImages
      },
      0);

  // --- Shaders ---
  auto* vfs = App::get_system<VFS>(EngineSystems::VFS);
  auto shaders_dir = vfs->resolve_physical_dir(VFS::APP_DIR, "Shaders");

  Slang slang = {};
  slang.create_session({.root_directory = shaders_dir,
                        .definitions = {
                            // {"CULLING_MESHLET_COUNT", std::to_string(Mesh::MAX_MESHLET_INDICES)},
                            // {"CULLING_TRIANGLE_COUNT", std::to_string(Mesh::MAX_MESHLET_PRIMITIVES)},
                            {"HISTOGRAM_THREADS_X", std::to_string(GPU::HISTOGRAM_THREADS_X)},
                            {"HISTOGRAM_THREADS_Y", std::to_string(GPU::HISTOGRAM_THREADS_Y)},
                        }});

  slang.create_pipeline(runtime,
                        "2d_forward_pipeline",
                        dslci_00,
                        {.path = shaders_dir + "/2d_forward.slang", .entry_points = {"vs_main", "ps_main"}});

  // --- Sky ---
  slang.create_pipeline(runtime,
                        "sky_transmittance_pipeline",
                        {},
                        {.path = shaders_dir + "/passes/sky_transmittance.slang", .entry_points = {"cs_main"}});

  slang.create_pipeline(runtime,
                        "sky_multiscatter_lut_pipeline",
                        {},
                        {.path = shaders_dir + "/passes/sky_multiscattering.slang", .entry_points = {"cs_main"}});

  slang.create_pipeline(runtime,
                        "sky_view_pipeline",
                        dslci_00,
                        {.path = shaders_dir + "/passes/sky_view.slang", .entry_points = {"cs_main"}});

  slang.create_pipeline(runtime,
                        "sky_aerial_perspective_pipeline",
                        dslci_00,
                        {.path = shaders_dir + "/passes/sky_aerial_perspective.slang", .entry_points = {"cs_main"}});

  slang.create_pipeline(runtime,
                        "sky_final_pipeline",
                        dslci_00,
                        {.path = shaders_dir + "/passes/sky_final.slang", .entry_points = {"vs_main", "fs_main"}});

  // --- PostProcess ---
  slang.create_pipeline(runtime,
                        "histogram_generate_pipeline",
                        {},
                        {.path = shaders_dir + "/passes/histogram_generate.slang", .entry_points = {"cs_main"}});

  slang.create_pipeline(runtime,
                        "histogram_average_pipeline",
                        {},
                        {.path = shaders_dir + "/passes/histogram_average.slang", .entry_points = {"cs_main"}});

  slang.create_pipeline(runtime,
                        "tonemap_pipeline",
                        {},
                        {.path = shaders_dir + "/passes/tonemap.slang", .entry_points = {"vs_main", "fs_main"}});

  // --- DescriptorSets ---
  this->descriptor_set_00 = runtime.create_persistent_descriptorset(
      allocator, *runtime.get_named_pipeline("2d_forward_pipeline"), 0, 64);

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

  const vuk::Sampler linear_sampler_clamped = runtime.acquire_sampler(vuk::LinearSamplerClamped,
                                                                      runtime.get_frame_count());
  const vuk::Sampler linear_sampler_repeated = runtime.acquire_sampler(vuk::LinearSamplerRepeated,
                                                                       runtime.get_frame_count());
  const vuk::Sampler linear_sampler_repeated_anisotropy = runtime.acquire_sampler(vuk::LinearSamplerRepeatedAnisotropy,
                                                                                  runtime.get_frame_count());
  const vuk::Sampler nearest_sampler_clamped = runtime.acquire_sampler(vuk::NearestSamplerClamped,
                                                                       runtime.get_frame_count());
  const vuk::Sampler nearest_sampler_repeated = runtime.acquire_sampler(vuk::NearestSamplerRepeated,
                                                                        runtime.get_frame_count());
  const vuk::Sampler hiz_sampler = runtime.acquire_sampler(hiz_sampler_ci, runtime.get_frame_count());
  this->descriptor_set_00->update_sampler(BindlessID::Samplers, 0, linear_sampler_repeated);
  this->descriptor_set_00->update_sampler(BindlessID::Samplers, 1, linear_sampler_clamped);

  this->descriptor_set_00->update_sampler(BindlessID::Samplers, 2, nearest_sampler_repeated);
  this->descriptor_set_00->update_sampler(BindlessID::Samplers, 3, nearest_sampler_clamped);

  this->descriptor_set_00->update_sampler(BindlessID::Samplers, 4, linear_sampler_repeated_anisotropy);

  this->descriptor_set_00->update_sampler(BindlessID::Samplers, 5, hiz_sampler);

  // const vuk::Sampler cmp_depth_sampler = runtime.acquire_sampler(vuk::CmpDepthSampler, runtime.get_frame_count());
  // this->descriptor_set_00->update_sampler(12, 0, cmp_depth_sampler);

  sky_transmittance_lut_view = Texture("sky_transmittance_lut");
  sky_transmittance_lut_view.create({},
                                    {.preset = vuk::ImageAttachment::Preset::eSTT2DUnmipped,
                                     .format = vuk::Format::eR16G16B16A16Sfloat,
                                     .extent = vuk::Extent3D{.width = 256u, .height = 64u, .depth = 1u}});

  sky_multiscatter_lut_view = Texture("sky_multiscatter_lut");
  sky_multiscatter_lut_view.create({},
                                   {.preset = vuk::ImageAttachment::Preset::eSTT2DUnmipped,
                                    .format = vuk::Format::eR16G16B16A16Sfloat,
                                    .extent = vuk::Extent3D{.width = 32u, .height = 32u, .depth = 1u}});

  auto temp_atmos_info = GPU::Atmosphere{};
  temp_atmos_info.transmittance_lut_size = sky_transmittance_lut_view.get_extent();
  temp_atmos_info.multiscattering_lut_size = sky_multiscatter_lut_view.get_extent();
  auto temp_atmos_buffer = vk_context.scratch_buffer(std::span(&temp_atmos_info, 1));

  auto transmittance_lut_attachment = sky_transmittance_lut_view.discard("sky_transmittance_lut");

  std::tie(transmittance_lut_attachment, temp_atmos_buffer) = vuk::make_pass(
      "transmittance_lut_pass",
      [](vuk::CommandBuffer& cmd_list, VUK_IA(vuk::eComputeRW) dst, VUK_BA(vuk::eComputeRead) atmos) {
        cmd_list.bind_compute_pipeline("sky_transmittance_pipeline")
            .bind_image(1, 0, dst)
            .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, PushConstants(atmos->device_address))
            .dispatch_invocations_per_pixel(dst);

        return std::make_tuple(dst, atmos);
      })(std::move(transmittance_lut_attachment), std::move(temp_atmos_buffer));

  auto multiscatter_lut_attachment = sky_multiscatter_lut_view.discard("sky_multiscatter_lut");

  std::tie(transmittance_lut_attachment, multiscatter_lut_attachment, temp_atmos_buffer) = vuk::make_pass(
      "sky_multiscatter_lut_pass",
      [](vuk::CommandBuffer& cmd_list,
         VUK_IA(vuk::eComputeSampled) sky_transmittance_lut,
         VUK_IA(vuk::eComputeRW) sky_multiscatter_lut,
         VUK_BA(vuk::eComputeRead) atmos) {
        cmd_list.bind_compute_pipeline("sky_multiscatter_lut_pipeline")
            .bind_sampler(1, 0, {.magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear})
            .bind_image(1, 1, sky_transmittance_lut)
            .bind_image(1, 2, sky_multiscatter_lut)
            .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, PushConstants(atmos->device_address))
            .dispatch_invocations_per_pixel(sky_multiscatter_lut);

        return std::make_tuple(sky_transmittance_lut, sky_multiscatter_lut, atmos);
      })(std::move(transmittance_lut_attachment), std::move(multiscatter_lut_attachment), std::move(temp_atmos_buffer));

  transmittance_lut_attachment = transmittance_lut_attachment.as_released(vuk::eComputeSampled,
                                                                          vuk::DomainFlagBits::eGraphicsQueue);
  multiscatter_lut_attachment = multiscatter_lut_attachment.as_released(vuk::eComputeSampled,
                                                                        vuk::DomainFlagBits::eGraphicsQueue);

  vk_context.wait_on(std::move(transmittance_lut_attachment));
  vk_context.wait_on(std::move(multiscatter_lut_attachment));
}

auto EasyRenderPipeline::shutdown() -> void {}

auto EasyRenderPipeline::on_render(VkContext& vk_context, const RenderInfo& render_info)
    -> vuk::Value<vuk::ImageAttachment> {
  OX_SCOPED_ZONE;

  const bool freeze_culling = static_cast<bool>(RendererCVar::cvar_freeze_culling_frustum.get());
  CameraComponent cam = freeze_culling ? frozen_camera : current_camera;

  auto camera_data = GPU::CameraData{
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

  auto camera_buffer = vk_context.scratch_buffer(std::span(&camera_data, 1));

  RenderQueue2D render_queue_2d = {};

  render_queue_2d.init();

  std::vector<GPU::Material> gpu_materials = {};

  std::vector<vuk::ImageView> texture_views = {};

  for (const auto& sprite_component : this->sprite_component_list) {
    const auto distance = glm::distance(glm::vec3(0.f, 0.f, cam.position.z),
                                        glm::vec3(0.f, 0.f, sprite_component.get_position().z));

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
      auto gpu_mat = GPU::Material::from_material(*material,
                                                  add_texture_if_exists(material->albedo_texture).value_or(~0_u32));
      gpu_mat.uv_offset = sprite_component.current_uv_offset.value_or(gpu_mat.uv_offset);
      gpu_materials.emplace_back(gpu_mat);

      render_queue_2d.add(sprite_component, gpu_materials.size() - 1, distance);
    }
  }

  for (u32 i = 0; i < texture_views.size(); i++) {
    this->descriptor_set_00->update_sampled_image(
        BindlessID::SampledImages, i, texture_views[i], vuk::ImageLayout::eReadOnlyOptimalKHR);
  }

  render_queue_2d.update();
  render_queue_2d.sort();
  this->sprite_component_list.clear();

  this->descriptor_set_00->commit(*vk_context.runtime);

  const auto final_attachment_ia = vuk::ImageAttachment{
      .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
      .extent = render_info.extent,
      .format = vuk::Format::eB10G11R11UfloatPack32,
      .sample_count = vuk::Samples::e1,
      .level_count = 1,
      .layer_count = 1,
  };
  auto final_attachment = vuk::clear_image(vuk::declare_ia("final_attachment", final_attachment_ia), vuk::Black<float>);

  auto result_attachment = vuk::declare_ia(
      "result",
      {.usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
       .format = render_info.format,
       .sample_count = vuk::Samples::e1});
  result_attachment.same_shape_as(final_attachment);
  result_attachment = vuk::clear_image(std::move(result_attachment), vuk::Black<f32>);

  const auto depth_ia = vuk::ImageAttachment{
      .extent = render_info.extent,
      .format = vuk::Format::eD32Sfloat,
      .sample_count = vuk::SampleCountFlagBits::e1,
      .level_count = 1,
      .layer_count = 1,
  };
  auto depth_attachment = vuk::clear_image(vuk::declare_ia("depth_image", depth_ia), vuk::DepthZero);

  auto material_buffer = vk_context.scratch_buffer(std::span(gpu_materials));
  auto vertex_buffer_2d = vk_context.scratch_buffer(std::span(render_queue_2d.sprite_data));

  if (!gpu_materials.empty() && !render_queue_2d.sprite_data.empty()) {
    std::tie(final_attachment, depth_attachment, camera_buffer, vertex_buffer_2d, material_buffer) = vuk::make_pass(
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
                    .depthWriteEnable = true,
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

          return std::make_tuple(target, depth, camera, vertex_buffer, materials);
        })(final_attachment, depth_attachment, vertex_buffer_2d, material_buffer, camera_buffer);
  }

  if (this->atmosphere.has_value()) {
    sky_pass(vk_context, camera_buffer, final_attachment, depth_attachment);
  }

  auto histogram_inf = this->histogram_info.value_or(GPU::HistogramInfo{});

  auto histogram_buffer = vk_context.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly,
                                                            GPU::HISTOGRAM_BIN_COUNT * sizeof(u32));
  vuk::fill(histogram_buffer, 0);

  std::tie(final_attachment, histogram_buffer) = vuk::make_pass(
      "histogram generate",
      [histogram_inf](vuk::CommandBuffer& cmd_list, VUK_IA(vuk::eComputeRead) src, VUK_BA(vuk::eComputeRW) histogram) {
        cmd_list.bind_compute_pipeline("histogram_generate_pipeline")
            .bind_image(0, 0, src)
            .push_constants(vuk::ShaderStageFlagBits::eCompute,
                            0,
                            PushConstants( //
                                histogram->device_address,
                                glm::uvec2(src->extent.width, src->extent.height),
                                histogram_inf.min_exposure,
                                1.0f / (histogram_inf.max_exposure - histogram_inf.min_exposure)))
            .dispatch_invocations_per_pixel(src);

        return std::make_tuple(src, histogram);
      })(std::move(final_attachment), std::move(histogram_buffer));

  auto exposure_buffer = vk_context.allocate_buffer(vuk::MemoryUsage::eGPUonly, sizeof(GPU::HistogramLuminance));
  auto exposure_buffer_value = vuk::acquire_buf("exposure buffer", *exposure_buffer, vuk::eNone);

  if (histogram_info.has_value()) {
    exposure_buffer_value = vuk::make_pass(
        "histogram average",
        [pixel_count = f32(final_attachment->extent.width * final_attachment->extent.height), histogram_inf](
            vuk::CommandBuffer& cmd_list, VUK_BA(vuk::eComputeRW) histogram, VUK_BA(vuk::eComputeRW) exposure) {
          cmd_list.bind_compute_pipeline("histogram_average_pipeline")
              .push_constants(
                  vuk::ShaderStageFlagBits::eCompute,
                  0,
                  PushConstants(histogram->device_address,
                                exposure->device_address,
                                pixel_count,
                                histogram_inf.min_exposure,
                                histogram_inf.max_exposure - histogram_inf.min_exposure,
                                glm::clamp(static_cast<f32>(1.0f - glm::exp(-histogram_inf.adaptation_speed *
                                                                            App::get()->get_timestep().get_millis())),
                                           0.0f,
                                           1.0f),
                                histogram_inf.ev100_bias))
              .dispatch(1);

          return exposure;
        })(std::move(histogram_buffer), std::move(exposure_buffer_value));
  }

  result_attachment = vuk::make_pass(
      "tonemap",
      [](vuk::CommandBuffer& cmd_list,
         VUK_IA(vuk::eColorWrite) dst,
         VUK_IA(vuk::eFragmentSampled) src,
         VUK_BA(vuk::eFragmentRead) exposure) {
        cmd_list.bind_graphics_pipeline("tonemap_pipeline")
            .set_rasterization({})
            .set_color_blend(dst, vuk::BlendPreset::eOff)
            .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
            .set_viewport(0, vuk::Rect2D::framebuffer())
            .set_scissor(0, vuk::Rect2D::framebuffer())
            .bind_sampler(0, 0, {.magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear})
            .bind_image(0, 1, src)
            .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, exposure->device_address)
            .draw(3, 1, 0, 0);

        return dst;
      })(std::move(result_attachment), std::move(final_attachment), std::move(exposure_buffer_value));

  return result_attachment;
}

auto EasyRenderPipeline::sky_pass(VkContext& vk_context,
                                  vuk::Value<vuk::Buffer>& camera_buffer,
                                  vuk::Value<vuk::ImageAttachment>& final_attachment,
                                  vuk::Value<vuk::ImageAttachment>& depth_attachment) -> void {
  OX_SCOPED_ZONE;

  const vuk::Extent3D sky_view_lut_extent = {.width = 312, .height = 192, .depth = 1};
  auto sky_view_lut_attachment = vuk::declare_ia(
      "sky_view_lut",
      {.image_type = vuk::ImageType::e2D,
       .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
       .extent = sky_view_lut_extent,
       .format = vuk::Format::eR16G16B16A16Sfloat,
       .sample_count = vuk::Samples::e1,
       .view_type = vuk::ImageViewType::e2D,
       .level_count = 1,
       .layer_count = 1});

  const vuk::Extent3D sky_aerial_perspective_lut_extent = {.width = 32, .height = 32, .depth = 32};
  auto sky_aerial_perspective_attachment = vuk::declare_ia(
      "sky aerial perspective",
      {.image_type = vuk::ImageType::e3D,
       .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
       .extent = sky_aerial_perspective_lut_extent,
       .sample_count = vuk::Samples::e1,
       .view_type = vuk::ImageViewType::e3D,
       .level_count = 1,
       .layer_count = 1});
  sky_aerial_perspective_attachment.same_format_as(sky_view_lut_attachment);

  this->atmosphere->sky_view_lut_size = sky_view_lut_extent;
  this->atmosphere->aerial_perspective_lut_size = sky_aerial_perspective_lut_extent;
  this->atmosphere->transmittance_lut_size = sky_transmittance_lut_view.get_extent();
  this->atmosphere->multiscattering_lut_size = sky_multiscatter_lut_view.get_extent();

  auto atmosphere_buffer = vk_context.scratch_buffer(this->atmosphere);
  auto sun_buffer = vk_context.scratch_buffer(this->sun);

  auto sky_transmittance_lut_attachment = sky_transmittance_lut_view.acquire("sky_transmittance_lut",
                                                                             vuk::Access::eComputeSampled);
  auto sky_multiscatter_lut_attachment = sky_multiscatter_lut_view.acquire("sky_multiscatter_lut",
                                                                           vuk::Access::eComputeSampled);

  std::tie(sky_view_lut_attachment,
           sky_transmittance_lut_attachment,
           sky_multiscatter_lut_attachment,
           atmosphere_buffer,
           sun_buffer,
           camera_buffer) = vuk::make_pass( //
      "sky view",
      [&descriptor_set = *this->descriptor_set_00](vuk::CommandBuffer& cmd_list,
                                                   VUK_BA(vuk::eComputeRead) atmosphere_,
                                                   VUK_BA(vuk::eComputeRead) sun_,
                                                   VUK_BA(vuk::eComputeRead) camera,
                                                   VUK_IA(vuk::eComputeSampled) sky_transmittance_lut,
                                                   VUK_IA(vuk::eComputeSampled) sky_multiscatter_lut,
                                                   VUK_IA(vuk::eComputeRW) sky_view_lut) {
        cmd_list.bind_compute_pipeline("sky_view_pipeline")
            .bind_persistent(0, descriptor_set)
            .bind_image(1, 0, sky_transmittance_lut)
            .bind_image(1, 1, sky_multiscatter_lut)
            .bind_image(1, 2, sky_view_lut)
            .push_constants(vuk::ShaderStageFlagBits::eCompute,
                            0,
                            PushConstants(atmosphere_->device_address, sun_->device_address, camera->device_address))
            .dispatch_invocations_per_pixel(sky_view_lut);

        return std::make_tuple(sky_view_lut, sky_transmittance_lut, sky_multiscatter_lut, atmosphere_, sun_, camera);
      })(std::move(atmosphere_buffer),
         std::move(sun_buffer),
         std::move(camera_buffer),
         std::move(sky_transmittance_lut_attachment),
         std::move(sky_multiscatter_lut_attachment),
         std::move(sky_view_lut_attachment));

  std::tie(sky_aerial_perspective_attachment,
           sky_transmittance_lut_attachment,
           atmosphere_buffer,
           sun_buffer,
           camera_buffer) = vuk::make_pass( //
      "sky aerial perspective",
      [&descriptor_set = *this->descriptor_set_00](vuk::CommandBuffer& cmd_list,
                                                   VUK_BA(vuk::eComputeRead) atmosphere_,
                                                   VUK_BA(vuk::eComputeRead) sun_,
                                                   VUK_BA(vuk::eComputeRead) camera,
                                                   VUK_IA(vuk::eComputeSampled) sky_transmittance_lut,
                                                   VUK_IA(vuk::eComputeSampled) sky_multiscatter_lut,
                                                   VUK_IA(vuk::eComputeRW) sky_aerial_perspective_lut) {
        cmd_list.bind_compute_pipeline("sky_aerial_perspective_pipeline")
            .bind_persistent(0, descriptor_set)
            .bind_image(1, 0, sky_transmittance_lut)
            .bind_image(1, 1, sky_multiscatter_lut)
            .bind_image(1, 2, sky_aerial_perspective_lut)
            .push_constants(vuk::ShaderStageFlagBits::eCompute,
                            0,
                            PushConstants(atmosphere_->device_address, sun_->device_address, camera->device_address))
            .dispatch_invocations_per_pixel(sky_aerial_perspective_lut);

        return std::make_tuple(sky_aerial_perspective_lut, sky_transmittance_lut, atmosphere_, sun_, camera);
      })(std::move(atmosphere_buffer),
         std::move(sun_buffer),
         std::move(camera_buffer),
         std::move(sky_transmittance_lut_attachment),
         std::move(sky_multiscatter_lut_attachment),
         std::move(sky_aerial_perspective_attachment));

  std::tie(final_attachment, depth_attachment, camera_buffer) = vuk::make_pass(
      "sky final",
      [&descriptor_set = *this->descriptor_set_00](vuk::CommandBuffer& cmd_list,
                                                   VUK_IA(vuk::eColorWrite) dst,
                                                   VUK_BA(vuk::eFragmentRead) atmosphere_,
                                                   VUK_BA(vuk::eFragmentRead) sun_,
                                                   VUK_BA(vuk::eFragmentRead) camera,
                                                   VUK_IA(vuk::eFragmentSampled) sky_transmittance_lut,
                                                   VUK_IA(vuk::eFragmentSampled) sky_aerial_perspective_lut,
                                                   VUK_IA(vuk::eFragmentSampled) sky_view_lut,
                                                   VUK_IA(vuk::eFragmentSampled) depth) {
        vuk::PipelineColorBlendAttachmentState blend_info = {
            .blendEnable = true,
            .srcColorBlendFactor = vuk::BlendFactor::eOne,
            .dstColorBlendFactor = vuk::BlendFactor::eSrcAlpha,
            .colorBlendOp = vuk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vuk::BlendFactor::eZero,
            .dstAlphaBlendFactor = vuk::BlendFactor::eOne,
            .alphaBlendOp = vuk::BlendOp::eAdd,
        };

        cmd_list.bind_graphics_pipeline("sky_final_pipeline")
            .bind_persistent(0, descriptor_set)
            .set_rasterization({})
            .set_depth_stencil({.depthTestEnable = false,
                                .depthWriteEnable = false,
                                .depthCompareOp = vuk::CompareOp::eGreaterOrEqual})
            .set_color_blend(dst, blend_info)
            .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
            .set_viewport(0, vuk::Rect2D::framebuffer())
            .set_scissor(0, vuk::Rect2D::framebuffer())
            .bind_image(1, 0, sky_transmittance_lut)
            .bind_image(1, 1, sky_aerial_perspective_lut)
            .bind_image(1, 2, sky_view_lut)
            .bind_image(1, 3, depth)
            .push_constants(vuk::ShaderStageFlagBits::eFragment,
                            0,
                            PushConstants(atmosphere_->device_address, sun_->device_address, camera->device_address))
            .draw(3, 1, 0, 0);

        return std::make_tuple(dst, depth, camera);
      })(std::move(final_attachment),
         std::move(atmosphere_buffer),
         std::move(sun_buffer),
         std::move(camera_buffer),
         std::move(sky_transmittance_lut_attachment),
         std::move(sky_aerial_perspective_attachment),
         std::move(sky_view_lut_attachment),
         std::move(depth_attachment));
}

auto EasyRenderPipeline::on_update(ox::Scene* scene) -> void {
  OX_SCOPED_ZONE;

  scene->world
      .query_builder<const TransformComponent, const CameraComponent>() //
      .build()
      .each([this](flecs::entity e, const TransformComponent& tc, const CameraComponent& c) {
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

        this->current_camera = c;
      });

  option<GPU::Atmosphere> atmosphere_data = nullopt;
  option<GPU::Sun> sun_data = nullopt;

  scene->world
      .query_builder<const TransformComponent, const LightComponent>() //
      .build()
      .each([&sun_data, &atmosphere_data, cam = &this->current_camera](
                flecs::entity e, const TransformComponent& tc, const LightComponent& lc) {
        if (lc.type == LightComponent::LightType::Directional) {
          auto& sund = sun_data.emplace();
          sund.direction.x = glm::cos(tc.rotation.x) * glm::sin(tc.rotation.y);
          sund.direction.y = glm::sin(tc.rotation.x) * glm::sin(tc.rotation.y);
          sund.direction.z = glm::cos(tc.rotation.y);
          sund.intensity = lc.intensity;
        }

        if (e.has<AtmosphereComponent>()) {
          const auto& atmos_info = *e.get<AtmosphereComponent>();
          auto& atmos = atmosphere_data.emplace();
          atmos.rayleigh_scatter = atmos_info.rayleigh_scattering * 1e-3f;
          atmos.rayleigh_density = atmos_info.rayleigh_density;
          atmos.mie_scatter = atmos_info.mie_scattering * 1e-3f;
          atmos.mie_density = atmos_info.mie_density;
          atmos.mie_extinction = atmos_info.mie_extinction * 1e-3f;
          atmos.mie_asymmetry = atmos_info.mie_asymmetry;
          atmos.ozone_absorption = atmos_info.ozone_absorption * 1e-3f;
          atmos.ozone_height = atmos_info.ozone_height;
          atmos.ozone_thickness = atmos_info.ozone_thickness;
          atmos.aerial_perspective_start_km = atmos_info.aerial_perspective_start_km;

          f32 eye_altitude = cam->position.y * GPU::CAMERA_SCALE_UNIT;
          eye_altitude += atmos.planet_radius + GPU::PLANET_RADIUS_OFFSET;
          atmos.eye_position = glm::vec3(0.0f, eye_altitude, 0.0f);
        }
      });

  this->atmosphere = atmosphere_data;
  this->sun = sun_data;

  scene->world
      .query_builder<const TransformComponent, const SpriteComponent>() //
      .build()
      .each([list = &this->sprite_component_list](
                flecs::entity e, const TransformComponent& tc, const SpriteComponent& c) { list->emplace_back(c); });

  option<GPU::HistogramInfo> hist_info = nullopt;

  scene->world
      .query_builder<const AutoExposureComponent>() //
      .build()
      .each([&hist_info](flecs::entity e, const AutoExposureComponent& c) {
        auto& i = hist_info.emplace();
        i.max_exposure = c.max_exposure;
        i.min_exposure = c.min_exposure;
        i.adaptation_speed = c.adaptation_speed;
        i.ev100_bias = c.ev100_bias;
      });

  this->histogram_info = hist_info;
}
} // namespace ox
