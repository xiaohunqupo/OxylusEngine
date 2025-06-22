#include "Render/RendererCommon.hpp"

#include <vuk/RenderGraph.hpp>
#include <vuk/vsl/Core.hpp>

#include "Core/App.hpp"
#include "Render/Utils/VukCommon.hpp"
#include "Render/Vulkan/VkContext.hpp"

namespace ox {
vuk::Value<vuk::ImageAttachment>
RendererCommon::generate_cubemap_from_equirectangular(vuk::Value<vuk::ImageAttachment> hdr_image) {
  auto& allocator = App::get_vkcontext().superframe_allocator;
  if (!allocator->get_context().is_pipeline_available("equirectangular_to_cubemap")) {
    vuk::PipelineBaseCreateInfo equirectangular_to_cubemap;
    // equirectangular_to_cubemap.add_glsl(fs::read_shader_file("Cubemap.vert"), "Cubemap.vert");
    // equirectangular_to_cubemap.add_glsl(fs::read_shader_file("EquirectangularToCubemap.frag"),
    // "EquirectangularToCubemap.frag");
    allocator->get_context().create_named_pipeline("equirectangular_to_cubemap", equirectangular_to_cubemap);
  }

  constexpr auto size = 2048;
  auto attch = vuk::ImageAttachment::from_preset(
      vuk::ImageAttachment::Preset::eRTTCube, vuk::Format::eR32G32B32A32Sfloat, {size, size, 1}, vuk::Samples::e1);
  auto target = vuk::clear_image(vuk::declare_ia("cubemap", attch), vuk::Black<float>);

  const auto capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
  const glm::mat4 capture_views[] = {
      lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
      lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
      lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
      lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
      lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
      lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

  auto cubemap_pass = vuk::make_pass("env_cubemap",
                                     [capture_projection, capture_views](vuk::CommandBuffer& command_buffer,
                                                                         VUK_IA(vuk::eColorWrite) cube_map,
                                                                         VUK_IA(vuk::eFragmentSampled) hdr) {
                                       command_buffer.set_viewport(0, vuk::Rect2D::framebuffer())
                                           .set_scissor(0, vuk::Rect2D::framebuffer())
                                           .broadcast_color_blend(vuk::BlendPreset::eOff)
                                           .set_rasterization({})
                                           .bind_image(0, 2, hdr)
                                           .bind_sampler(0, 2, vuk::LinearSamplerClamped)
                                           .bind_graphics_pipeline("equirectangular_to_cubemap");

                                       auto* projection = command_buffer.scratch_buffer<glm::mat4>(0, 0);
                                       *projection = capture_projection;
                                       const auto view = command_buffer.scratch_buffer<glm::mat4[6]>(0, 1);
                                       memcpy(view, capture_views, sizeof(capture_views));

                                       // const auto cube = generate_cube();
                                       // cube->bind_vertex_buffer(command_buffer);
                                       // cube->bind_index_buffer(command_buffer);
                                       // command_buffer.draw_indexed(cube->index_count, 6, 0, 0, 0);

                                       return cube_map;
                                     });

  auto envmap_output = cubemap_pass(hdr_image.mip(0), target);

  auto converge = vuk::make_pass(
      "converge", [](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eComputeRW) output) { return output; });
  return vuk::generate_mips(converge(envmap_output), target->level_count);
}

vuk::Value<vuk::ImageAttachment> RendererCommon::apply_blur(const vuk::Value<vuk::ImageAttachment>& src_attachment,
                                                            const vuk::Value<vuk::ImageAttachment>& dst_attachment) {
  auto& allocator = *App::get_vkcontext().superframe_allocator;
  if (!allocator.get_context().is_pipeline_available("gaussian_blur_pipeline")) {
    vuk::PipelineBaseCreateInfo pci;
    // pci.add_hlsl(fs::read_shader_file("FullscreenTriangle.hlsl"), fs::get_shader_path("FullscreenTriangle.hlsl"),
    // vuk::HlslShaderStage::eVertex); pci.add_glsl(fs::read_shader_file("PostProcess/SinglePassGaussianBlur.frag"),
    // "PostProcess/SinglePassGaussianBlur.frag");
    allocator.get_context().create_named_pipeline("gaussian_blur_pipeline", pci);
  }

  auto pass = vuk::make_pass(
      "blur", [](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eFragmentSampled) src, VUK_IA(vuk::eColorRW) target) {
        command_buffer.bind_graphics_pipeline("gaussian_blur_pipeline")
            .set_viewport(0, vuk::Rect2D::framebuffer())
            .set_scissor(0, vuk::Rect2D::framebuffer())
            .broadcast_color_blend(vuk::BlendPreset::eOff)
            .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
            .bind_image(0, 0, src)
            .bind_sampler(0, 0, vuk::LinearSamplerClamped)
            .draw(3, 1, 0, 0);

        return target;
      });

  return pass(src_attachment, dst_attachment);
}
} // namespace ox
