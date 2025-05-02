#pragma once
#include "Components.hpp"
#include "Render/RenderPipeline.hpp"

namespace ox {
class Scene;

class SceneRenderer {
public:
  SceneRenderer() = default;
  ~SceneRenderer() = default;

  void init(Scene* scene,
            const Shared<RenderPipeline>& render_pipeline = nullptr);
  void update(const Timestep& delta_time) const;

  const Shared<RenderPipeline>& get_render_pipeline() const { return _render_pipeline; }

private:
  Scene* _scene;
  Shared<RenderPipeline> _render_pipeline = nullptr;

  friend class Scene;
};
} // namespace ox
