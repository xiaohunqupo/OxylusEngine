#pragma once

#include <flecs.h>

#include "Core/UUID.hpp"
#include "EditorPanel.hpp"

namespace ox {
struct Material;
class Scene;
class InspectorPanel : public EditorPanel {
public:
  InspectorPanel();

  void on_render(vuk::Extent3D extent,
                 vuk::Format format) override;

  static void draw_material_properties(Material* material,
                                       const UUID& uuid,
                                       flecs::entity load_event);

private:
  void draw_components(flecs::entity entity);

  flecs::entity selected_entity = {};
  Scene* _scene;
  bool _rename_entity = false;
};
} // namespace ox
