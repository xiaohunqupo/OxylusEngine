#pragma once
#include <mutex>
#include <vuk/Value.hpp>

#include "Event/Event.hpp"

#include "Core/Base.hpp"
#include "Scene/Components.hpp"

namespace vuk {
struct SampledImage;
}

namespace ox {
class VkContext;
class Scene;

class RenderPipeline {
public:
  RenderPipeline(std::string name) : _name(std::move(name)), attach_swapchain(true) {}

  virtual ~RenderPipeline() = default;

  virtual void init(vuk::Allocator& allocator) = 0;
  virtual void shutdown() = 0;

  [[nodiscard]] virtual vuk::Value<vuk::ImageAttachment> on_render(vuk::Allocator& frame_allocator, vuk::Extent3D ext, vuk::Format format) = 0;

  virtual void on_dispatcher_events(EventDispatcher& dispatcher) {}

  virtual void on_update(Scene* scene) {}
  virtual void on_submit() {} // TODO: Not called anymore!! Old Code!!
  virtual void submit_mesh_component(const MeshComponent& render_object) {}
  virtual void submit_light(const LightComponent& light) {}
  virtual void submit_camera(Camera* camera) {}
  virtual void submit_sprite(const SpriteComponent& sprite) {}

  virtual const std::string& get_name() { return _name; }

protected:
  std::string _name = {};
  bool attach_swapchain = false;
  vuk::Extent3D _extent = {};
  vuk::Value<vuk::ImageAttachment>* final_image = nullptr;
  vuk::Allocator* _frame_allocator;
  vuk::Compiler _compiler = {};
  std::mutex setup_lock;
};
} // namespace ox
