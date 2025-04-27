#pragma once
#include "Components.hpp"
#include "Render/RenderPipeline.hpp"

namespace ox {
class Scene;

class SceneRenderer {
public:
  SceneRenderer(Scene* scene) : _scene(scene) {}
  ~SceneRenderer() = default;

  void init();
  void update(const Timestep& delta_time) const;

  const Unique<RenderPipeline>& get_render_pipeline() const { return _render_pipeline; }

private:
  Scene* _scene;
  Unique<RenderPipeline> _render_pipeline = nullptr;

  friend class Scene;
};
}
