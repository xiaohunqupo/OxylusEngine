#pragma once

#include <imgui_internal.h>

#include "EditorPanel.hpp"
#include "Scene/Scene.hpp"
#include "UI/SceneHierarchyViewer.hpp"

namespace ox {
class SceneHierarchyPanel : public EditorPanel {
public:
  SceneHierarchyPanel();

  auto on_update() -> void override;
  auto on_render(vuk::Extent3D extent, vuk::Format format) -> void override;

  auto set_scene(Scene* scene) -> void { viewer.set_scene(scene); }
  auto get_scene() -> Scene* { return viewer.get_scene(); }

private:
  SceneHierarchyViewer viewer = {};
};
} // namespace ox
