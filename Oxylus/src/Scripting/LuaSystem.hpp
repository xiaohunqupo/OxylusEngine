#pragma once

#include <entt/entity/entity.hpp>

#include <sol/environment.hpp>
#include <vuk/Types.hpp>

#include "Core/Systems/System.hpp"

namespace ox {
class LuaSystem {
public:
  LuaSystem(std::string path);
  ~LuaSystem() = default;

  void load(const std::string& path);
  void reload();

  void bind_globals(Scene* scene, entt::entity entity, const Timestep& timestep);

  void on_init(Scene* scene, entt::entity entity);
  void on_update(const Timestep& delta_time);
  void on_release(Scene* scene, entt::entity entity);
  void on_render(vuk::Extent3D extent, vuk::Format format);

  const std::string& get_path() const { return file_path; }

private:
  std::string file_path;
  ankerl::unordered_dense::map<int, std::string> errors = {};

  Unique<sol::environment> environment = nullptr;
  Unique<sol::protected_function> on_init_func = nullptr;
  Unique<sol::protected_function> on_release_func = nullptr;
  Unique<sol::protected_function> on_update_func = nullptr;
  Unique<sol::protected_function> on_render_func = nullptr;
  Unique<sol::protected_function> on_fixed_update_func = nullptr;

  void init_script(const std::string& path);
  void check_result(const sol::protected_function_result& result, const char* func_name);
};
}
