#include "ImGuiLayer.hpp"
#include <imgui.h>

#include <SDL3/SDL_mouse.h>
#include <icons/IconsMaterialDesignIcons.h>
#include <icons/MaterialDesign.inl>
#include <vuk/RenderGraph.hpp>
#include <vuk/runtime/CommandBuffer.hpp>
#include <vuk/runtime/vk/AllocatorHelpers.hpp>
#include <vuk/runtime/vk/Pipeline.hpp>
#include <vuk/vsl/Core.hpp>

#include "Core/App.hpp"
#include "ImGuizmo.h"
#include "Utils/Profiler.hpp"
#include "imgui_frag.hpp"
#include "imgui_vert.hpp"

#include "Render/Utils/VukCommon.hpp"
#include "Render/Vulkan/VkContext.hpp"
#include "Render/Window.hpp"

namespace ox {
static ImVec4 darken(ImVec4 c, float p) { return {glm::max(0.f, c.x - 1.0f * p), glm::max(0.f, c.y - 1.0f * p), glm::max(0.f, c.z - 1.0f * p), c.w}; }
static ImVec4 lighten(ImVec4 c, float p) {
  return {glm::max(0.f, c.x + 1.0f * p), glm::max(0.f, c.y + 1.0f * p), glm::max(0.f, c.z + 1.0f * p), c.w};
}

ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {}

ImFont* ImGuiLayer::load_font(const std::string& path, ImFontConfig font_config) {
  OX_SCOPED_ZONE_N("Font Loading");

  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->TexGlyphPadding = 1;
  return io.Fonts->AddFontFromFileTTF(path.c_str(), font_config.SizePixels, &font_config);
}

void ImGuiLayer::build_fonts() {
  OX_SCOPED_ZONE_N("Font Building");

  ImGuiIO& io = ImGui::GetIO();
  unsigned char* pixels;
  int width, height;
  io.Fonts->Build();
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  font_texture = create_shared<Texture>();
  font_texture->create_texture({.width = static_cast<unsigned>(width), .height = static_cast<unsigned>(height), .depth = 1},
                               pixels,
                               vuk::Format::eR8G8B8A8Srgb,
                               Preset::eRTT2DUnmipped);
}

void ImGuiLayer::on_attach(EventDispatcher&) {
  OX_SCOPED_ZONE;
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard /*| ImGuiConfigFlags_ViewportsEnable*/ | ImGuiConfigFlags_DockingEnable |
                    ImGuiConfigFlags_DpiEnableScaleFonts;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_HasMouseCursors;
  /*io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;*/
  io.BackendRendererName = "oxylus";
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

  auto& allocator = *App::get_vkcontext().superframe_allocator;
  auto& ctx = allocator.get_context();
  vuk::PipelineBaseCreateInfo pci;
  pci.add_static_spirv(imgui_vert, sizeof(imgui_vert) / 4, "imgui.vert");
  pci.add_static_spirv(imgui_frag, sizeof(imgui_frag) / 4, "imgui.frag");
  ctx.create_named_pipeline("imgui", pci);

  apply_theme();
  set_style();
}

void ImGuiLayer::add_icon_font(float font_size) {
  OX_SCOPED_ZONE;
  const ImGuiIO& io = ImGui::GetIO();
  static constexpr ImWchar ICONS_RANGES[] = {ICON_MIN_MDI, ICON_MAX_MDI, 0};
  ImFontConfig icons_config;
  // merge in icons from Font Awesome
  icons_config.MergeMode = true;
  icons_config.PixelSnapH = true;
  icons_config.GlyphOffset.y = 0.5f;
  icons_config.OversampleH = icons_config.OversampleV = 3;
  icons_config.GlyphMinAdvanceX = 4.0f;
  icons_config.SizePixels = font_size;

  io.Fonts->AddFontFromMemoryCompressedTTF(MaterialDesign_compressed_data, MaterialDesign_compressed_size, font_size, &icons_config, ICONS_RANGES);
}

void ImGuiLayer::on_detach() { ImGui::DestroyContext(); }

void ImGuiLayer::begin_frame(const float64 delta_time, const vuk::Extent3D extent) {
  OX_SCOPED_ZONE;

  const App* app = App::get();
  auto& imgui = ImGui::GetIO();
  imgui.DeltaTime = static_cast<float32>(delta_time);
  imgui.DisplaySize = ImVec2(static_cast<float32>(extent.width), static_cast<float32>(extent.height));

  rendering_images.clear();
  acquired_images.clear();

  add_image(font_texture->acquire());

  ImGui::NewFrame();
  ImGuizmo::BeginFrame();

  if (imgui.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) {
    return;
  }

  const auto imgui_cursor = ImGui::GetMouseCursor();
  if (imgui.MouseDrawCursor || imgui_cursor == ImGuiMouseCursor_None) {
    app->get_window().show_cursor(false);
  } else {
    auto next_cursor = WindowCursor::Arrow;
    // clang-format off
        switch (imgui_cursor) {
            case ImGuiMouseCursor_Arrow: next_cursor = WindowCursor::Arrow; break;
            case ImGuiMouseCursor_TextInput: next_cursor = WindowCursor::TextInput; break;
            case ImGuiMouseCursor_ResizeAll: next_cursor = WindowCursor::ResizeAll; break;
            case ImGuiMouseCursor_ResizeNS: next_cursor = WindowCursor::ResizeNS; break;
            case ImGuiMouseCursor_ResizeEW: next_cursor = WindowCursor::ResizeEW; break;
            case ImGuiMouseCursor_ResizeNESW: next_cursor = WindowCursor::ResizeNESW; break;
            case ImGuiMouseCursor_ResizeNWSE: next_cursor = WindowCursor::ResizeNWSE; break;
            case ImGuiMouseCursor_Hand: next_cursor = WindowCursor::Hand; break;
            case ImGuiMouseCursor_NotAllowed: next_cursor = WindowCursor::NotAllowed; break;
            default: break;
        }
    // clang-format on
    app->get_window().show_cursor(true);

    if (app->get_window().get_cursor() != next_cursor) {
      app->get_window().set_cursor(next_cursor);
    }
  }
}

vuk::Value<vuk::ImageAttachment> ImGuiLayer::end_frame(vuk::Allocator& allocator, vuk::Value<vuk::ImageAttachment> target) {
  OX_SCOPED_ZONE;

  ImGui::Render();

  ImDrawData* draw_data = ImGui::GetDrawData();

  auto reset_render_state = [this, draw_data](vuk::CommandBuffer& command_buffer, const vuk::Buffer& vertex, const vuk::Buffer& index) {
    command_buffer.bind_image(0, 0, *font_texture->get_view()).bind_sampler(0, 0, vuk::LinearSamplerRepeated);
    if (index.size > 0) {
      command_buffer.bind_index_buffer(index, sizeof(ImDrawIdx) == 2 ? vuk::IndexType::eUint16 : vuk::IndexType::eUint32);
    }
    command_buffer.bind_vertex_buffer(0, vertex, 0, vuk::Packed{vuk::Format::eR32G32Sfloat, vuk::Format::eR32G32Sfloat, vuk::Format::eR8G8B8A8Unorm});
    command_buffer.bind_graphics_pipeline("imgui");
    command_buffer.set_viewport(0, vuk::Rect2D::framebuffer());
    struct PC {
      float scale[2];
      float translate[2];
    } pc;
    pc.scale[0] = 2.0f / draw_data->DisplaySize.x;
    pc.scale[1] = 2.0f / draw_data->DisplaySize.y;
    pc.translate[0] = -1.0f - draw_data->DisplayPos.x * pc.scale[0];
    pc.translate[1] = -1.0f - draw_data->DisplayPos.y * pc.scale[1];
    command_buffer.push_constants(vuk::ShaderStageFlagBits::eVertex, 0, pc);
  };

  size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
  size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
  auto imvert = *allocate_buffer(allocator, {vuk::MemoryUsage::eCPUtoGPU, vertex_size, 1});
  auto imind = *allocate_buffer(allocator, {vuk::MemoryUsage::eCPUtoGPU, index_size, 1});

  size_t vtx_dst = 0, idx_dst = 0;
  for (int n = 0; n < draw_data->CmdListsCount; n++) {
    const ImDrawList* cmd_list = draw_data->CmdLists[n];
    auto imverto = imvert->add_offset(vtx_dst * sizeof(ImDrawVert));
    auto imindo = imind->add_offset(idx_dst * sizeof(ImDrawIdx));

    memcpy(imverto.mapped_ptr, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
    memcpy(imindo.mapped_ptr, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));

    vtx_dst += cmd_list->VtxBuffer.Size;
    idx_dst += cmd_list->IdxBuffer.Size;
  }

  auto sampled_images_array = vuk::declare_array("imgui_sampled", std::span(rendering_images));

  return vuk::make_pass("imgui",
                        [verts = imvert.get(), inds = imind.get(), reset_render_state, draw_data](vuk::CommandBuffer& command_buffer,
                                                                                                  VUK_IA(vuk::eColorWrite) color_rt,
                                                                                                  VUK_ARG(vuk::ImageAttachment[],
                                                                                                          vuk::Access::eFragmentSampled) sis) {
    command_buffer.set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
      .set_rasterization(vuk::PipelineRasterizationStateCreateInfo{})
      .set_color_blend(color_rt, vuk::BlendPreset::eAlphaBlend);

    reset_render_state(command_buffer, verts, inds);
    // Will project scissor/clipping rectangles into framebuffer space
    const ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    const ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    for (int n = 0; n < draw_data->CmdListsCount; n++) {
      const ImDrawList* cmd_list = draw_data->CmdLists[n];
      for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
        const ImDrawCmd* im_cmd = &cmd_list->CmdBuffer[cmd_i];
        if (im_cmd->UserCallback != nullptr) {
          // User callback, registered via ImDrawList::AddCallback()
          // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
          if (im_cmd->UserCallback == ImDrawCallback_ResetRenderState)
            reset_render_state(command_buffer, verts, inds);
          else
            im_cmd->UserCallback(cmd_list, im_cmd);
        } else {
          // Project scissor/clipping rectangles into framebuffer space
          ImVec4 clip_rect;
          clip_rect.x = (im_cmd->ClipRect.x - clip_off.x) * clip_scale.x;
          clip_rect.y = (im_cmd->ClipRect.y - clip_off.y) * clip_scale.y;
          clip_rect.z = (im_cmd->ClipRect.z - clip_off.x) * clip_scale.x;
          clip_rect.w = (im_cmd->ClipRect.w - clip_off.y) * clip_scale.y;

          const auto fb_width = command_buffer.get_ongoing_render_pass().extent.width;
          const auto fb_height = command_buffer.get_ongoing_render_pass().extent.height;
          if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
            // Negative offsets are illegal for vkCmdSetScissor
            clip_rect.x = std::max(clip_rect.x, 0.0f);
            clip_rect.y = std::max(clip_rect.y, 0.0f);

            // Apply scissor/clipping rectangle
            vuk::Rect2D scissor;
            scissor.offset.x = static_cast<int32_t>(clip_rect.x);
            scissor.offset.y = static_cast<int32_t>(clip_rect.y);
            scissor.extent.width = static_cast<uint32_t>(clip_rect.z - clip_rect.x);
            scissor.extent.height = static_cast<uint32_t>(clip_rect.w - clip_rect.y);
            command_buffer.set_scissor(0, scissor);

            command_buffer.bind_sampler(0, 0, {.magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear});
            if (im_cmd->TextureId != 0) {
              const auto index = im_cmd->TextureId - 1;
              const auto& image = sis[index];
              command_buffer.bind_image(0, 0, image);
            } else {
              command_buffer.bind_image(0, 0, sis[0]);
            }

            command_buffer.draw_indexed(im_cmd->ElemCount, 1, im_cmd->IdxOffset + global_idx_offset, im_cmd->VtxOffset + global_vtx_offset, 0);
          }
        }
      }
      global_idx_offset += cmd_list->IdxBuffer.Size;
      global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    return color_rt;
  })(std::move(target), std::move(sampled_images_array));
}

ImTextureID ImGuiLayer::add_image(vuk::Value<vuk::ImageAttachment> attachment) {
  rendering_images.emplace_back(std::move(attachment));
  return rendering_images.size();
}

ImTextureID ImGuiLayer::add_image(const Texture& texture) {
  if (this->acquired_images.contains(texture.get_view_id())) {
    return this->acquired_images[texture.get_view_id()];
  }

  const auto attachment = texture.acquire();
  const auto texture_id = this->add_image(attachment);
  this->acquired_images.emplace(texture.get_view_id(), texture_id);

  return texture_id;
}

void ImGuiLayer::on_mouse_pos(glm::vec2 pos) {
  ZoneScoped;

  auto& imgui = ImGui::GetIO();
  imgui.AddMousePosEvent(pos.x, pos.y);
}

void ImGuiLayer::on_mouse_button(uint8 button, bool down) {
  ZoneScoped;

  int32 imgui_button = 0;
  // clang-format off
    switch (button) {
        case SDL_BUTTON_LEFT: imgui_button = 0; break;
        case SDL_BUTTON_RIGHT: imgui_button = 1; break;
        case SDL_BUTTON_MIDDLE: imgui_button = 2; break;
        case SDL_BUTTON_X1: imgui_button = 3; break;
        case SDL_BUTTON_X2: imgui_button = 4; break;
        default: return;
    }
  // clang-format on

  auto& imgui = ImGui::GetIO();
  imgui.AddMouseButtonEvent(imgui_button, down);
  imgui.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
}

void ImGuiLayer::on_mouse_scroll(glm::vec2 offset) {
  ZoneScoped;

  auto& imgui = ImGui::GetIO();
  imgui.AddMouseWheelEvent(offset.x, offset.y);
}

ImGuiKey to_imgui_key(SDL_Keycode keycode, SDL_Scancode scancode);
void ImGuiLayer::on_key(uint32 key_code, uint32 scan_code, uint16 mods, bool down) {
  ZoneScoped;

  auto& imgui = ImGui::GetIO();
  imgui.AddKeyEvent(ImGuiMod_Ctrl, (mods & SDL_KMOD_CTRL) != 0);
  imgui.AddKeyEvent(ImGuiMod_Shift, (mods & SDL_KMOD_SHIFT) != 0);
  imgui.AddKeyEvent(ImGuiMod_Alt, (mods & SDL_KMOD_ALT) != 0);
  imgui.AddKeyEvent(ImGuiMod_Super, (mods & SDL_KMOD_GUI) != 0);

  const auto key = to_imgui_key(static_cast<SDL_Keycode>(key_code), static_cast<SDL_Scancode>(scan_code));
  imgui.AddKeyEvent(key, down);
  imgui.SetKeyEventNativeData(key, static_cast<int32>(key_code), static_cast<int32>(scan_code), static_cast<int32>(scan_code));
}

void ImGuiLayer::on_text_input(const char8* text) {
  ZoneScoped;

  auto& imgui = ImGui::GetIO();
  imgui.AddInputCharactersUTF8(text);
}

ImGuiKey to_imgui_key(SDL_Keycode keycode, SDL_Scancode scancode) {
  ZoneScoped;

  switch (scancode) {
    case SDL_SCANCODE_KP_0       : return ImGuiKey_Keypad0;
    case SDL_SCANCODE_KP_1       : return ImGuiKey_Keypad1;
    case SDL_SCANCODE_KP_2       : return ImGuiKey_Keypad2;
    case SDL_SCANCODE_KP_3       : return ImGuiKey_Keypad3;
    case SDL_SCANCODE_KP_4       : return ImGuiKey_Keypad4;
    case SDL_SCANCODE_KP_5       : return ImGuiKey_Keypad5;
    case SDL_SCANCODE_KP_6       : return ImGuiKey_Keypad6;
    case SDL_SCANCODE_KP_7       : return ImGuiKey_Keypad7;
    case SDL_SCANCODE_KP_8       : return ImGuiKey_Keypad8;
    case SDL_SCANCODE_KP_9       : return ImGuiKey_Keypad9;
    case SDL_SCANCODE_KP_PERIOD  : return ImGuiKey_KeypadDecimal;
    case SDL_SCANCODE_KP_DIVIDE  : return ImGuiKey_KeypadDivide;
    case SDL_SCANCODE_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
    case SDL_SCANCODE_KP_MINUS   : return ImGuiKey_KeypadSubtract;
    case SDL_SCANCODE_KP_PLUS    : return ImGuiKey_KeypadAdd;
    case SDL_SCANCODE_KP_ENTER   : return ImGuiKey_KeypadEnter;
    case SDL_SCANCODE_KP_EQUALS  : return ImGuiKey_KeypadEqual;
    default                      : break;
  }

  switch (keycode) {
    case SDLK_TAB         : return ImGuiKey_Tab;
    case SDLK_LEFT        : return ImGuiKey_LeftArrow;
    case SDLK_RIGHT       : return ImGuiKey_RightArrow;
    case SDLK_UP          : return ImGuiKey_UpArrow;
    case SDLK_DOWN        : return ImGuiKey_DownArrow;
    case SDLK_PAGEUP      : return ImGuiKey_PageUp;
    case SDLK_PAGEDOWN    : return ImGuiKey_PageDown;
    case SDLK_HOME        : return ImGuiKey_Home;
    case SDLK_END         : return ImGuiKey_End;
    case SDLK_INSERT      : return ImGuiKey_Insert;
    case SDLK_DELETE      : return ImGuiKey_Delete;
    case SDLK_BACKSPACE   : return ImGuiKey_Backspace;
    case SDLK_SPACE       : return ImGuiKey_Space;
    case SDLK_RETURN      : return ImGuiKey_Enter;
    case SDLK_ESCAPE      : return ImGuiKey_Escape;
    case SDLK_APOSTROPHE  : return ImGuiKey_Apostrophe;
    case SDLK_COMMA       : return ImGuiKey_Comma;
    case SDLK_MINUS       : return ImGuiKey_Minus;
    case SDLK_PERIOD      : return ImGuiKey_Period;
    case SDLK_SLASH       : return ImGuiKey_Slash;
    case SDLK_SEMICOLON   : return ImGuiKey_Semicolon;
    case SDLK_EQUALS      : return ImGuiKey_Equal;
    case SDLK_LEFTBRACKET : return ImGuiKey_LeftBracket;
    case SDLK_BACKSLASH   : return ImGuiKey_Backslash;
    case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
    case SDLK_GRAVE       : return ImGuiKey_GraveAccent;
    case SDLK_CAPSLOCK    : return ImGuiKey_CapsLock;
    case SDLK_SCROLLLOCK  : return ImGuiKey_ScrollLock;
    case SDLK_NUMLOCKCLEAR: return ImGuiKey_NumLock;
    case SDLK_PRINTSCREEN : return ImGuiKey_PrintScreen;
    case SDLK_PAUSE       : return ImGuiKey_Pause;
    case SDLK_LCTRL       : return ImGuiKey_LeftCtrl;
    case SDLK_LSHIFT      : return ImGuiKey_LeftShift;
    case SDLK_LALT        : return ImGuiKey_LeftAlt;
    case SDLK_LGUI        : return ImGuiKey_LeftSuper;
    case SDLK_RCTRL       : return ImGuiKey_RightCtrl;
    case SDLK_RSHIFT      : return ImGuiKey_RightShift;
    case SDLK_RALT        : return ImGuiKey_RightAlt;
    case SDLK_RGUI        : return ImGuiKey_RightSuper;
    case SDLK_APPLICATION : return ImGuiKey_Menu;
    case SDLK_0           : return ImGuiKey_0;
    case SDLK_1           : return ImGuiKey_1;
    case SDLK_2           : return ImGuiKey_2;
    case SDLK_3           : return ImGuiKey_3;
    case SDLK_4           : return ImGuiKey_4;
    case SDLK_5           : return ImGuiKey_5;
    case SDLK_6           : return ImGuiKey_6;
    case SDLK_7           : return ImGuiKey_7;
    case SDLK_8           : return ImGuiKey_8;
    case SDLK_9           : return ImGuiKey_9;
    case SDLK_A           : return ImGuiKey_A;
    case SDLK_B           : return ImGuiKey_B;
    case SDLK_C           : return ImGuiKey_C;
    case SDLK_D           : return ImGuiKey_D;
    case SDLK_E           : return ImGuiKey_E;
    case SDLK_F           : return ImGuiKey_F;
    case SDLK_G           : return ImGuiKey_G;
    case SDLK_H           : return ImGuiKey_H;
    case SDLK_I           : return ImGuiKey_I;
    case SDLK_J           : return ImGuiKey_J;
    case SDLK_K           : return ImGuiKey_K;
    case SDLK_L           : return ImGuiKey_L;
    case SDLK_M           : return ImGuiKey_M;
    case SDLK_N           : return ImGuiKey_N;
    case SDLK_O           : return ImGuiKey_O;
    case SDLK_P           : return ImGuiKey_P;
    case SDLK_Q           : return ImGuiKey_Q;
    case SDLK_R           : return ImGuiKey_R;
    case SDLK_S           : return ImGuiKey_S;
    case SDLK_T           : return ImGuiKey_T;
    case SDLK_U           : return ImGuiKey_U;
    case SDLK_V           : return ImGuiKey_V;
    case SDLK_W           : return ImGuiKey_W;
    case SDLK_X           : return ImGuiKey_X;
    case SDLK_Y           : return ImGuiKey_Y;
    case SDLK_Z           : return ImGuiKey_Z;
    case SDLK_F1          : return ImGuiKey_F1;
    case SDLK_F2          : return ImGuiKey_F2;
    case SDLK_F3          : return ImGuiKey_F3;
    case SDLK_F4          : return ImGuiKey_F4;
    case SDLK_F5          : return ImGuiKey_F5;
    case SDLK_F6          : return ImGuiKey_F6;
    case SDLK_F7          : return ImGuiKey_F7;
    case SDLK_F8          : return ImGuiKey_F8;
    case SDLK_F9          : return ImGuiKey_F9;
    case SDLK_F10         : return ImGuiKey_F10;
    case SDLK_F11         : return ImGuiKey_F11;
    case SDLK_F12         : return ImGuiKey_F12;
    case SDLK_F13         : return ImGuiKey_F13;
    case SDLK_F14         : return ImGuiKey_F14;
    case SDLK_F15         : return ImGuiKey_F15;
    case SDLK_F16         : return ImGuiKey_F16;
    case SDLK_F17         : return ImGuiKey_F17;
    case SDLK_F18         : return ImGuiKey_F18;
    case SDLK_F19         : return ImGuiKey_F19;
    case SDLK_F20         : return ImGuiKey_F20;
    case SDLK_F21         : return ImGuiKey_F21;
    case SDLK_F22         : return ImGuiKey_F22;
    case SDLK_F23         : return ImGuiKey_F23;
    case SDLK_F24         : return ImGuiKey_F24;
    case SDLK_AC_BACK     : return ImGuiKey_AppBack;
    case SDLK_AC_FORWARD  : return ImGuiKey_AppForward;
    default               : break;
  }
  return ImGuiKey_None;
}

void ImGuiLayer::apply_theme(bool dark) {
  ImVec4* colors = ImGui::GetStyle().Colors;

  if (dark) {
    colors[ImGuiCol_Text] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.178f, 0.178f, 0.178f, 1.000f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.178f, 0.178f, 0.178f, 1.000f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.43f, 0.43f, 0.43f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 0.56f, 0.00f, 0.82f);
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(1.00f, 0.56f, 0.00f, 0.22f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.85f, 0.48f, 0.00f, 0.73f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

    header_selected_color = ImVec4(1.00f, 0.56f, 0.00f, 0.50f);
    header_hovered_color = lighten(colors[ImGuiCol_HeaderActive], 0.1f);
    window_bg_color = colors[ImGuiCol_WindowBg];
    window_bg_alternative_color = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    asset_icon_color = lighten(header_selected_color, 0.9f);
    text_color = colors[ImGuiCol_Text];
    text_disabled_color = colors[ImGuiCol_TextDisabled];
  } else {
    colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.40f, 0.40f, 0.40f, 0.30f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.40f, 0.40f, 0.40f, 0.30f);
    colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.70f, 0.82f, 0.95f, 0.39f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.86f, 0.86f, 0.86f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.49f, 0.49f, 0.49f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.41f, 0.67f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.70f, 0.82f, 0.95f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.89f, 0.89f, 0.89f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.81f, 0.81f, 0.81f, 0.62f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.56f, 0.56f, 0.56f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.81f, 0.81f, 0.81f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.22f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.78f, 0.87f, 0.98f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.57f, 0.57f, 0.64f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.68f, 0.68f, 0.74f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.30f, 0.30f, 0.30f, 0.09f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

    header_selected_color = ImVec4(0.26f, 0.59f, 0.98f, 0.65f);
    header_hovered_color = darken(colors[ImGuiCol_HeaderActive], 0.1f);
    window_bg_color = colors[ImGuiCol_WindowBg];
    window_bg_alternative_color = darken(window_bg_color, 0.04f);
    asset_icon_color = darken(header_selected_color, 0.9f);
    text_color = colors[ImGuiCol_Text];
    text_disabled_color = colors[ImGuiCol_TextDisabled];
  }
}

void ImGuiLayer::set_style() {
  {
    auto& style = ImGuizmo::GetStyle();
    style.TranslationLineThickness *= 1.3f;
    style.TranslationLineArrowSize *= 1.3f;
    style.RotationLineThickness *= 1.3f;
    style.RotationOuterLineThickness *= 1.3f;
    style.ScaleLineThickness *= 1.3f;
    style.ScaleLineCircleSize *= 1.3f;
    style.HatchedAxisLineThickness *= 1.3f;
    style.CenterCircleSize *= 1.3f;

    ImGuizmo::SetGizmoSizeClipSpace(0.2f);
  }

  {
    ImGuiStyle* style = &ImGui::GetStyle();

    style->AntiAliasedFill = true;
    style->AntiAliasedLines = true;
    style->AntiAliasedLinesUseTex = true;

    style->WindowPadding = ImVec2(4.0f, 4.0f);
    style->FramePadding = ImVec2(4.0f, 4.0f);
    style->CellPadding = ImVec2(8.0f, 4.0f);
    style->ItemSpacing = ImVec2(8.0f, 3.0f);
    style->ItemInnerSpacing = ImVec2(2.0f, 4.0f);
    style->TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style->IndentSpacing = 12;
    style->ScrollbarSize = 14;
    style->GrabMinSize = 10;

    style->WindowBorderSize = 0.0f;
    style->ChildBorderSize = 0.0f;
    style->PopupBorderSize = 1.5f;
    style->FrameBorderSize = 0.0f;
    style->TabBorderSize = 1.0f;
    style->DockingSeparatorSize = 3.0f;

    style->WindowRounding = 6.0f;
    style->ChildRounding = 0.0f;
    style->FrameRounding = 2.0f;
    style->PopupRounding = 2.0f;
    style->ScrollbarRounding = 3.0f;
    style->GrabRounding = 2.0f;
    style->LogSliderDeadzone = 4.0f;
    style->TabRounding = 3.0f;

    style->WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style->WindowMenuButtonPosition = ImGuiDir_None;
    style->ColorButtonPosition = ImGuiDir_Left;
    style->ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style->SelectableTextAlign = ImVec2(0.0f, 0.0f);
    style->DisplaySafeAreaPadding = ImVec2(8.0f, 8.0f);

    ui_frame_padding = ImVec2(4.0f, 2.0f);
    popup_item_spacing = ImVec2(6.0f, 8.0f);

    constexpr ImGuiColorEditFlags color_edit_flags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf |
                                                     ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB |
                                                     ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_Uint8;
    ImGui::SetColorEditOptions(color_edit_flags);

    style->ScaleAllSizes(1.0f);
  }
}
} // namespace ox
