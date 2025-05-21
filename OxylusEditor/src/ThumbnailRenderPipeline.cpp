#include "ThumbnailRenderPipeline.hpp"

#include "Render/Camera.hpp"
#include "Render/Slang/Slang.hpp"
#include "Scene/SceneGPU.hpp"

namespace ox {
auto ThumbnailRenderPipeline::init(VkContext& vk_context) -> void {
  auto& runtime = *vk_context.runtime;
  auto& allocator = *vk_context.superframe_allocator;

  // --- Shaders ---
  auto* vfs = App::get_system<VFS>(EngineSystems::VFS);
  auto shaders_dir = vfs->resolve_physical_dir(VFS::APP_DIR, "Shaders");

  Slang slang = {};
  slang.create_session({.root_directory = shaders_dir, .definitions = {}});

  slang.create_pipeline(runtime,
                        "simple_forward_pipeline",
                        {},
                        {.path = shaders_dir + "editor/3d_forward.slang", .entry_points = {"vs_main", "ps_main"}});
}

auto ThumbnailRenderPipeline::shutdown() -> void {}

auto ThumbnailRenderPipeline::on_render(VkContext& vk_context, const RenderInfo& render_info)
    -> vuk::Value<vuk::ImageAttachment> {

  const auto final_attachment_ia = vuk::ImageAttachment{
      .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
      .extent = render_info.extent,
      .format = vuk::Format::eB10G11R11UfloatPack32,
      .sample_count = vuk::Samples::e1,
      .level_count = 1,
      .layer_count = 1,
  };

  auto final_attachment = vuk::clear_image(vuk::declare_ia(thumbnail_name.c_str(), final_attachment_ia),
                                           vuk::Black<float>);

  if (mesh == nullptr)
    return final_attachment;

  CameraComponent cam{};
  Camera::update(cam, {render_info.extent.width, render_info.extent.height});

  const auto camera_data = GPU::CameraData{
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

  auto camera_buffer = vk_context.scratch_buffer(std::span(&camera_data, 1));

  auto thumbnail_pass = vuk::make_pass( //
      "thumbnail_pass",
      [](vuk::CommandBuffer& command_buffer,
         VUK_IA(vuk::eFragmentWrite) output,
         VUK_BA(vuk::eVertexRead) camera_buffer_) {
        command_buffer.bind_graphics_pipeline("simple_forward_pipeline")
            .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
            .set_viewport(0, vuk::Rect2D::framebuffer())
            .set_scissor(0, vuk::Rect2D::framebuffer())
            .set_rasterization({})
            .broadcast_color_blend({})
            .bind_buffer(0, 0, camera_buffer_);
        return output;
      });

  return thumbnail_pass(final_attachment, camera_buffer);
}

auto ThumbnailRenderPipeline::on_update(Scene* scene) -> void {}

auto ThumbnailRenderPipeline::set_mesh(this ThumbnailRenderPipeline& self, Mesh* mesh) -> void {
  OX_SCOPED_ZONE;

  self.mesh = mesh;
}

auto ThumbnailRenderPipeline::set_name(this ThumbnailRenderPipeline& self, const std::string& name) -> void {
  OX_SCOPED_ZONE;

  self.thumbnail_name = name;
}
} // namespace ox
