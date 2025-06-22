#pragma once

#include <flecs.h>
#include <sol/environment.hpp>
#include <vuk/Types.hpp>

#include "Oxylus.hpp"

namespace JPH {
class ContactSettings;
class ContactManifold;
class Body;
} // namespace JPH

namespace ox {
class Scene;

enum class ScriptID : u64 { Invalid = std::numeric_limits<u64>::max() };
class LuaSystem {
public:
  LuaSystem() = default;
  explicit LuaSystem(std::string path);
  ~LuaSystem() = default;

  auto load(const std::string& path) -> void;
  auto reload() -> void;

  auto bind_globals(Scene* scene, flecs::entity entity, f32 delta_time) const -> void;

  auto on_init(Scene* scene, flecs::entity entity) -> void;
  auto on_update(f32 delta_time) -> void;
  auto on_fixed_update(float delta_time) -> void;
  auto on_release(Scene* scene, flecs::entity entity) -> void;
  auto on_render(vuk::Extent3D extent, vuk::Format format) -> void;

  auto get_path() const -> const std::string& { return file_path; }

private:
  std::string file_path = {};
  ankerl::unordered_dense::map<int, std::string> errors = {};

  std::unique_ptr<sol::environment> environment = nullptr;
  std::unique_ptr<sol::protected_function> on_init_func = nullptr;
  std::unique_ptr<sol::protected_function> on_release_func = nullptr;
  std::unique_ptr<sol::protected_function> on_update_func = nullptr;
  std::unique_ptr<sol::protected_function> on_render_func = nullptr;
  std::unique_ptr<sol::protected_function> on_fixed_update_func = nullptr;

  void init_script(const std::string& path);
  void check_result(const sol::protected_function_result& result, const char* func_name);
};
} // namespace ox
