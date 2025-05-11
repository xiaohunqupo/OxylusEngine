#pragma once
#include <ankerl/unordered_dense.h>
#include <imgui.h>

struct ImFont;

namespace ox {
class EditorTheme {
public:
  ImFont* regular_font = nullptr;
  ImFont* small_font = nullptr;
  ImFont* bold_font = nullptr;
  ImFont* big_icons = nullptr;

  inline static ImVec4 header_selected_color;
  inline static ImVec4 header_hovered_color;
  inline static ImVec4 window_bg_alternative_color;
  inline static ImVec4 asset_icon_color;

  inline static ImVec2 ui_frame_padding;
  inline static ImVec2 popup_item_spacing;

  ankerl::unordered_dense::map<size_t, const char8_t*> component_icon_map = {};

  void init(this EditorTheme& self);

  void apply_theme(bool dark = true);
  void set_style();
};
} // namespace ox
