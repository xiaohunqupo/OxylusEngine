#pragma once

#include <flecs.h>

#include "Asset/PBRMaterial.hpp"
#include "Asset/SpriteMaterial.hpp"
#include "EditorPanel.hpp"

namespace ox {
class Scene;
class InspectorPanel : public EditorPanel {
public:
  InspectorPanel();

  void on_render(vuk::Extent3D extent, vuk::Format format) override;

  static void draw_pbr_material_properties(Shared<PBRMaterial>& material);
  static void draw_sprite_material_properties(Shared<SpriteMaterial>& material);

private:
  void draw_components(flecs::entity entity);

  flecs::entity selected_entity = {};
  Scene* _scene;
  bool _rename_entity = false;
};
} // namespace ox
