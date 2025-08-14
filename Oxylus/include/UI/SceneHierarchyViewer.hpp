#pragma once

#include <imgui_internal.h>

#include "Scene/Scene.hpp"

namespace ox {
class SceneHierarchyViewer {
public:
  class SelectedEntity {
  public:
    std::function<void(flecs::entity)> on_selected_entity_callback = {};
    std::function<void()> on_selected_entity_reset_callback = {};

    auto set(this SelectedEntity& self, flecs::entity e) -> void {
      self.entity = e;

      if (self.on_selected_entity_callback)
        self.on_selected_entity_callback(e);
    }

    auto get(this SelectedEntity& self) -> flecs::entity { return self.entity; };

    auto reset(this SelectedEntity& self) -> void {
      self.entity = flecs::entity::null();

      if (self.on_selected_entity_reset_callback)
        self.on_selected_entity_reset_callback();
    }

  private:
    flecs::entity entity = flecs::entity::null();
  };

  SelectedEntity selected_entity_ = {};

  bool table_hovered_ = false;
  bool window_hovered_ = false;

  flecs::entity renaming_entity_ = {};
  flecs::entity dragged_entity_ = {};
  flecs::entity dragged_entity_target_ = {};
  flecs::entity deleted_entity_ = {};

  const char* add_entity_icon = "Add";
  const char* search_icon = "";
  const char* entity_icon = "";
  const char* visibility_icon_on = "V";
  const char* visibility_icon_off = "NV";

  ImVec4 header_selected_color = ImVec4(1.00f, 0.56f, 0.00f, 0.50f);
  ImVec2 popup_item_spacing = ImVec2(6.0f, 8.0f);

  SceneHierarchyViewer() = default;
  SceneHierarchyViewer(Scene* scene);

  auto render(const char* id, bool* visible) -> void;

  auto on_selected_entity_callback(const std::function<void(flecs::entity)> callback) -> void;
  auto on_selected_entity_reset_callback(const std::function<void()> callback) -> void;

  auto set_scene(Scene* scene) -> void;
  auto get_scene() -> Scene*;

private:
  Scene* scene_ = nullptr;

  ImGuiTextFilter filter_;

  auto draw_entity_node(flecs::entity entity,
                        uint32_t depth = 0,
                        bool force_expand_tree = false,
                        bool is_part_of_prefab = false) -> ImRect;

  auto draw_context_menu() -> void;
};
} // namespace ox
