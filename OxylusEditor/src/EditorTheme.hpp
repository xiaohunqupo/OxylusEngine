#pragma once
#include <ankerl/unordered_dense.h>

struct ImFont;

namespace ox {
class EditorTheme {
public:
  ImFont* regular_font = nullptr;
  ImFont* small_font = nullptr;
  ImFont* bold_font = nullptr;

  ankerl::unordered_dense::map<size_t, const char8_t*> component_icon_map = {};

  void init(this EditorTheme& self);
};
} // namespace ox
