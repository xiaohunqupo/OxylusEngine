#include "App.hpp"

#include <filesystem>
#include <ranges>
#include <vuk/vsl/Core.hpp>

#include "Assets/AssetManager.hpp"
#include "FileSystem.hpp"
#include "Layer.hpp"
#include "LayerStack.hpp"
#include "Physics/Physics.hpp"
#include "Project.hpp"
#include "VFS.hpp"

#include "Audio/AudioEngine.hpp"

#include "Modules/ModuleRegistry.hpp"

#include "Render/RendererConfig.hpp"
#include "Render/Window.hpp"

#include "Scripting/LuaManager.hpp"

#include "Systems/SystemManager.hpp"
#include "Thread/TaskScheduler.hpp"
#include "Thread/ThreadManager.hpp"

#include "UI/ImGuiLayer.hpp"

#include "Utils/Profiler.hpp"
#include "Utils/Random.hpp"

namespace ox {
App* App::_instance = nullptr;

App::App(const AppSpec& spec) : app_spec(spec) {
  OX_SCOPED_ZONE;
  if (_instance) {
    OX_LOG_ERROR("Application already exists!");
    return;
  }

  _instance = this;

  layer_stack = create_shared<LayerStack>();
  thread_manager = create_shared<ThreadManager>();

  if (app_spec.working_directory.empty())
    app_spec.working_directory = std::filesystem::current_path().string();
  else
    std::filesystem::current_path(app_spec.working_directory);

  register_system<AssetManager>(EngineSystems::AssetManager);
  register_system<VFS>(EngineSystems::VFS);
  register_system<Random>(EngineSystems::Random);
  register_system<TaskScheduler>(EngineSystems::TaskScheduler);
  register_system<AudioEngine>(EngineSystems::AudioEngine);
  register_system<LuaManager>(EngineSystems::LuaManager);
  register_system<ModuleRegistry>(EngineSystems::ModuleRegistry);
  register_system<RendererConfig>(EngineSystems::RendererConfig);
  register_system<SystemManager>(EngineSystems::SystemManager);
  register_system<Physics>(EngineSystems::Physics);

  window = Window::create(app_spec.window_info);

  register_system<Input>(EngineSystems::Input);

  for (const auto& system : system_registry | std::views::values) {
    system->set_dispatcher(&dispatcher);
    system->init();
  }

  // Shortcut for commonly used Systems
  AssetManager::set_instance();
  Input::set_instance();
  Physics::set_instance();

  const bool enable_validation = app_spec.command_line_args.contains("vulkan-validation");
  vk_context.create_context(window, enable_validation);

  DebugRenderer::init();

  imgui_layer = new ImGuiLayer();
  push_overlay(imgui_layer);
}

App::~App() { close(); }

void App::set_instance(App* instance) {
  _instance = instance;
  get_system<AssetManager>(EngineSystems::AssetManager)->set_instance();
  get_system<Input>(EngineSystems::Input)->set_instance();
  get_system<Physics>(EngineSystems::Physics)->set_instance();
}

App& App::push_layer(Layer* layer) {
  layer_stack->push_layer(layer);
  layer->on_attach(dispatcher);

  return *this;
}

App& App::push_overlay(Layer* layer) {
  layer_stack->push_overlay(layer);
  layer->on_attach(dispatcher);

  return *this;
}

void App::run() {
  const auto input_sys = get_system<Input>(EngineSystems::Input);

  WindowCallbacks window_callbacks = {};
  window_callbacks.user_data = this;
  window_callbacks.on_resize = [](void* user_data, const glm::uvec2 size) {
    const auto app = static_cast<App*>(user_data);
    app->vk_context.handle_resize(size.x, size.y);
  };
  window_callbacks.on_close = [](void* user_data) {
    const auto app = static_cast<App*>(user_data);
    app->is_running = false;
  };
  window_callbacks.on_mouse_pos = [](void* user_data, const glm::vec2 position, [[maybe_unused]] glm::vec2 relative) {
    const auto* app = static_cast<App*>(user_data);
    app->imgui_layer->on_mouse_pos(position);

    const auto input_system = get_system<Input>(EngineSystems::Input);
    input_system->input_data.mouse_offset_x = input_system->input_data.mouse_pos.x - position.x;
    input_system->input_data.mouse_offset_y = input_system->input_data.mouse_pos.y - position.y;
    input_system->input_data.mouse_pos = position;
    input_system->input_data.mouse_pos_rel = relative;
    input_system->input_data.mouse_moved = true;
  };
  window_callbacks.on_mouse_button = [](void* user_data, const uint8 button, const bool down) {
    const auto* app = static_cast<App*>(user_data);
    app->imgui_layer->on_mouse_button(button, down);

    const auto input_system = get_system<Input>(EngineSystems::Input);
    const auto ox_button = Input::to_mouse_code(button);
    if (down) {
      input_system->set_mouse_clicked(ox_button, true);
      input_system->set_mouse_held(ox_button, true);
    } else {
      input_system->set_mouse_clicked(ox_button, false);
      input_system->set_mouse_held(ox_button, false);
    }
  };
  window_callbacks.on_mouse_scroll = [](void* user_data, const glm::vec2 offset) {
    const auto* app = static_cast<App*>(user_data);
    app->imgui_layer->on_mouse_scroll(offset);

    const auto input_system = get_system<Input>(EngineSystems::Input);
    input_system->input_data.scroll_offset_y = offset.y;
  };
  window_callbacks.on_key =
    [](void* user_data, const SDL_Keycode key_code, const SDL_Scancode scan_code, const uint16 mods, const bool down, const bool repeat) {
    const auto* app = static_cast<App*>(user_data);
    app->imgui_layer->on_key(key_code, scan_code, mods, down);

    const auto input_system = get_system<Input>(EngineSystems::Input);
    const auto ox_key_code = Input::to_keycode(key_code, scan_code);
    if (down) {
      input_system->set_key_pressed(ox_key_code, !repeat);
      input_system->set_key_released(ox_key_code, false);
      input_system->set_key_held(ox_key_code, true);
    } else {
      input_system->set_key_pressed(ox_key_code, false);
      input_system->set_key_released(ox_key_code, true);
      input_system->set_key_held(ox_key_code, false);
    }
  };
  window_callbacks.on_text_input = [](void* user_data, const char8* text) {
    const auto* app = static_cast<App*>(user_data);
    app->imgui_layer->on_text_input(text);
  };

  while (is_running) {
    timestep.on_update();

    window.poll(window_callbacks);

    auto swapchain_attachment = vk_context.new_frame();
    swapchain_attachment = vuk::clear_image(std::move(swapchain_attachment), vuk::Black<float32>);

    const auto extent = swapchain_attachment->extent;
    this->swapchain_extent = glm::vec2{extent.width, extent.height};

    imgui_layer->begin_frame(timestep.get_seconds(), extent);

    for (Layer* layer : *layer_stack.get()) {
      layer->on_update(timestep);
      layer->on_render(extent, swapchain_attachment->format);
    }

    for (const auto& system : system_registry | std::views::values) {
      system->on_update();
      system->on_render(extent, swapchain_attachment->format);
    }

    auto& frame_allocator = vk_context.get_frame_allocator();

    swapchain_attachment = imgui_layer->end_frame(frame_allocator.value(), std::move(swapchain_attachment));

    vk_context.end_frame(frame_allocator.value(), swapchain_attachment);

    input_sys->reset_pressed();
  }

  layer_stack.reset();

  if (Project::get_active())
    Project::get_active()->unload_module();

  for (const auto& system : system_registry | std::views::values)
    system->deinit();

  system_registry.clear();

  DebugRenderer::release();

  ThreadManager::get()->wait_all_threads();
  window.destroy();
}

void App::close() { is_running = false; }

glm::vec2 App::get_swapchain_extent() const { return this->swapchain_extent; }

bool App::asset_directory_exists() { return std::filesystem::exists(get_asset_directory()); }

std::string App::get_asset_directory() {
  if (Project::get_active() && !Project::get_active()->get_config().asset_directory.empty())
    return Project::get_asset_directory();
  return _instance->app_spec.assets_path;
}

std::string App::get_asset_directory(const std::string_view asset_path) { return fs::append_paths(get_asset_directory(), asset_path); }

std::string App::get_asset_directory_absolute() {
  if (Project::get_active()) {
    const auto p = std::filesystem::absolute(Project::get_asset_directory());
    return p.string();
  }
  const auto p = absolute(std::filesystem::path(_instance->app_spec.assets_path));
  return p.string();
}

std::string App::get_absolute(const std::string& path) { return fs::append_paths(fs::preferred_path(get_asset_directory_absolute()), path); }
} // namespace ox
