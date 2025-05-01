#pragma once

#include <imgui_internal.h>

#include "EditorPanel.hpp"
#include "Scene/Scene.hpp"

namespace ox {
class SceneHierarchyPanel : public EditorPanel {
public:
  SceneHierarchyPanel();

  auto on_update() -> void override;
  auto on_render(vuk::Extent3D extent, vuk::Format format) -> void override;

  auto draw_entity_node(flecs::entity entity, uint32_t depth = 0, bool force_expand_tree = false, bool is_part_of_prefab = false) -> ImRect;
  auto set_scene(const Shared<Scene>& scene) -> void;

  auto get_selected_entity() const -> flecs::entity;
  auto clear_selected_entity() -> void { _selected_entity = flecs::entity::null(); }
  auto set_selected_entity(flecs::entity entity) -> void;
  auto get_scene() const -> const Shared<Scene>& { return _scene; }

  auto drag_drop_target() const -> void;

private:
  Shared<Scene> _scene = nullptr;
  ImGuiTextFilter _filter;
  bool _table_hovered = false;
  bool _window_hovered = false;
  flecs::entity _selected_entity = {};
  flecs::entity _renaming_entity = {};
  flecs::entity _dragged_entity = {};
  flecs::entity _dragged_entity_target = {};
  flecs::entity _deleted_entity = {};

  void draw_context_menu();
};
} // namespace ox
