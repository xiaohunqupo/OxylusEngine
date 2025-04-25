#pragma once
#include <exception>
#include <optional>
#include <string>

#include <ankerl/unordered_dense.h>

#include "Base.hpp"
#include "Core/Types.hpp"
#include "ESystem.hpp"
#include "Render/Window.hpp"

#include "Render/Vulkan/VkContext.hpp"
#include "Utils/Log.hpp"
#include "Utils/Timestep.hpp"

int main(int argc, char** argv);

namespace ox {
class Layer;
class LayerStack;
class ImGuiLayer;
class ThreadManager;

struct AppCommandLineArgs {
  struct Arg {
    std::string arg_str;
    uint32 arg_index;
  };

  std::vector<Arg> args = {};

  AppCommandLineArgs() = default;
  AppCommandLineArgs(const int argc, char** argv) {
    for (int i = 0; i < argc; i++)
      args.emplace_back(Arg{.arg_str = argv[i], .arg_index = (uint32)i});
  }

  bool contains(const std::string_view arg) const {
    for (const auto& a : args) {
      if (a.arg_str == arg) {
        return true;
      }
    }
    return false;
  }

  std::optional<Arg> get(const uint32 index) const {
    try {
      return args.at(index);
    } catch ([[maybe_unused]] std::exception& exception) {
      return std::nullopt;
    }
  }

  std::optional<uint32> get_index(const std::string_view arg) const {
    for (const auto& a : args) {
      if (a.arg_str == arg) {
        return a.arg_index;
      }
    }
    return std::nullopt;
  }
};

struct AppSpec {
  std::string name = "Oxylus App";
  std::string working_directory = {};
  std::string assets_path = "Resources";
  uint32_t device_index = 0;
  AppCommandLineArgs command_line_args = {};
  WindowInfo window_info = {};
};

enum class EngineSystems {
  AssetManager = 0,
  VFS,
  Random,
  TaskScheduler,
  AudioEngine,
  LuaManager,
  ModuleRegistry,
  RendererConfig,
  SystemManager,
  Physics,
  Input,

  Count,
};

using SystemRegistry = ankerl::unordered_dense::map<EngineSystems, Shared<ESystem>>;

class App {
public:
  App(const AppSpec& spec);
  virtual ~App();

  static App* get() { return _instance; }
  static void set_instance(App* instance);

  void close();

  App& push_layer(Layer* layer);
  App& push_overlay(Layer* layer);

  const AppSpec& get_specification() const { return app_spec; }
  const AppCommandLineArgs& get_command_line_args() const { return app_spec.command_line_args; }

  ImGuiLayer* get_imgui_layer() const { return imgui_layer; }
  const Shared<LayerStack>& get_layer_stack() const { return layer_stack; }

  const Window& get_window() const { return window; }
  static VkContext& get_vkcontext() { return _instance->vk_context; }
  glm::vec2 get_swapchain_extent() const;

  static const Timestep& get_timestep() { return _instance->timestep; }

  static bool asset_directory_exists();

  // TODO: Get rid off these
  static std::string get_asset_directory();
  static std::string get_asset_directory(std::string_view asset_path); // appends the asset_path at the end
  static std::string get_asset_directory_absolute();
  static std::string get_absolute(const std::string& path);

  static SystemRegistry& get_system_registry() { return _instance->system_registry; }

  template <typename T, typename... Args>
  static void register_system(const EngineSystems type, Args&&... args) {
    if (_instance->system_registry.contains(type)) {
      OX_LOG_ERROR("Registering system more than once.");
      return;
    }

    Shared<T> system = create_shared<T>(std::forward<Args>(args)...);
    _instance->system_registry.emplace(type, std::move(system));
  }

  static void unregister_system(const EngineSystems type) {
    if (_instance->system_registry.contains(type)) {
      _instance->system_registry.erase(type);
    }
  }

  template <typename T>
  static T* get_system(const EngineSystems type) {
    if (_instance->system_registry.contains(type)) {
      return dynamic_cast<T*>(_instance->system_registry[type].get());
    }

    return nullptr;
  }

  static bool has_system(const EngineSystems type) {
    return _instance->system_registry.contains(type);
  }

private:
  static App* _instance;
  AppSpec app_spec;
  ImGuiLayer* imgui_layer;
  Shared<LayerStack> layer_stack;
  VkContext vk_context;
  Window window;
  glm::vec2 swapchain_extent = {};

  SystemRegistry system_registry = {};
  EventDispatcher dispatcher;

  Shared<ThreadManager> thread_manager;
  Timestep timestep;

  bool is_running = true;
  float last_frame_time = 0.0f;

  void run();

  friend int ::main(int argc, char** argv);
};

App* create_application(const AppCommandLineArgs& args);
} // namespace ox
