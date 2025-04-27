#pragma once

#include <vuk/Value.hpp>

#include <imgui.h>

#include "Assets/Texture.hpp"
#include "Core/Layer.hpp"

namespace ox {
class ImGuiLayer : public Layer {
public:
  struct ImGuiImage {
    bool global;
    vuk::ImageView view;
    uint32_t attachment_index;
    bool linear_sampling = true;
  };

  Shared<Texture> font_texture = nullptr;
  std::vector<vuk::Value<vuk::ImageAttachment>> rendering_images;
  ankerl::unordered_dense::map<uint64, ImTextureID> acquired_images;

  inline static ImVec4 header_selected_color;
  inline static ImVec4 header_hovered_color;
  inline static ImVec4 window_bg_color;
  inline static ImVec4 window_bg_alternative_color;
  inline static ImVec4 asset_icon_color;
  inline static ImVec4 text_color;
  inline static ImVec4 text_disabled_color;

  inline static ImVec2 ui_frame_padding;
  inline static ImVec2 popup_item_spacing;

  ImGuiLayer();
  ~ImGuiLayer() override = default;

  void on_attach() override;
  void on_detach() override;

  void begin_frame(float64 delta_time, vuk::Extent3D extent);
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> end_frame(vuk::Allocator& allocator, vuk::Value<vuk::ImageAttachment> target);

  ImTextureID add_image(vuk::Value<vuk::ImageAttachment> attachment);
  ImTextureID add_image(const Texture& texture);

  ImFont* load_font(const std::string& path, ImFontConfig font_config);
  void add_icon_font(float font_size);
  void build_fonts();
  
  void on_mouse_pos(glm::vec2 pos);
  void on_mouse_button(uint8 button, bool down);
  void on_mouse_scroll(glm::vec2 offset);
  void on_key(uint32 key_code, uint32 scan_code, uint16 mods, bool down);
  void on_text_input(const char8 *text);

  static void apply_theme(bool dark = true);
  static void set_style();
};
} // namespace ox
