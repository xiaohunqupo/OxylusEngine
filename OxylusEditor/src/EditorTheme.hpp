#pragma once
#include <ankerl/unordered_dense.h>
#include <flecs.h>

struct ImFont;

namespace ox {
class EditorTheme {
public:
  // used for icon mapping
  flecs::world component_map;

  ImFont* regular_font = nullptr;
  ImFont* small_font = nullptr;
  ImFont* bold_font = nullptr;

  ankerl::unordered_dense::map<size_t, const char8_t*> component_icon_map = {};

  void init(this EditorTheme& self);
};
} // namespace ox
