#include "InspectorPanel.hpp"

#include <icons/IconsMaterialDesignIcons.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Asset/AssetManager.hpp"
#include "Core/App.hpp"
#include "Core/FileSystem.hpp"
#include "Core/SystemManager.hpp"
#include "EditorLayer.hpp"
#include "EditorTheme.hpp"
#include "Panels/ContentPanel.hpp"
#include "Render/ParticleSystem.hpp"
#include "Scene/ECSModule/Core.hpp"
#include "UI/OxUI.hpp"
#include "Utils/ColorUtils.hpp"
#include "Utils/StringUtils.hpp"

namespace ox {
InspectorPanel::InspectorPanel() :
    EditorPanel("Inspector",
                ICON_MDI_INFORMATION,
                true),
    _scene(nullptr) {}

void InspectorPanel::on_render(vuk::Extent3D extent,
                               vuk::Format format) {
  selected_entity = EditorLayer::get()->get_selected_entity();
  _scene = EditorLayer::get()->get_selected_scene().get();

  on_begin();
  if (selected_entity != flecs::entity::null()) {
    draw_components(selected_entity);
  }

  // OxUI::draw_gradient_shadow_bottom();

  on_end();
}

template <typename T,
          typename UIFunction>
void draw_component(const char* name,
                    flecs::entity entity,
                    UIFunction ui_function,
                    const bool removable = true) {
  if (!entity.has<T>())
    return;
  auto* component = entity.get_mut<T>();
  if (!component) {
    return;
  }
  static constexpr ImGuiTreeNodeFlags TREE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                                   ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed |
                                                   ImGuiTreeNodeFlags_FramePadding;

  const float line_height = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;

  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + line_height * 0.25f);

  auto& editor_theme = EditorLayer::get()->editor_theme;
  const auto id = typeid(T).hash_code();
  OX_ASSERT(editor_theme.component_icon_map.contains(id));
  std::string name_str = StringUtils::from_char8_t(editor_theme.component_icon_map[id]);
  name_str = name_str.append(name);
  const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(id), TREE_FLAGS, "%s", name_str.c_str());

  bool remove_component = false;
  if (removable) {
    ImGui::PushID(static_cast<int>(id));

    const float frame_height = ImGui::GetFrameHeight();
    ImGui::SameLine(ImGui::GetContentRegionMax().x - frame_height * 1.2f);
    if (ui::button(StringUtils::from_char8_t(ICON_MDI_SETTINGS), ImVec2{frame_height * 1.2f, frame_height}))
      ImGui::OpenPopup("ComponentSettings");

    if (ImGui::BeginPopup("ComponentSettings")) {
      if (ImGui::MenuItem("Remove Component"))
        remove_component = true;

      ImGui::EndPopup();
    }

    ImGui::PopID();
  }

  if (open) {
    ui_function(*component, entity);
    ImGui::TreePop();
  }

  if (remove_component)
    entity.remove<T>();
}

void InspectorPanel::draw_material_properties(Material* material,
                                              const UUID& uuid,
                                              flecs::entity load_event) {
  if (uuid) {
    const auto& window = App::get()->get_window();
    static auto uuid_copy = uuid;
    static auto load_event_copy = load_event;

    auto uuid_str = fmt::format("UUID: {}", uuid.str());
    ImGui::TextUnformatted(uuid_str.c_str());

    auto load_str = fmt::format("{} Load", StringUtils::from_char8_t(ICON_MDI_FILE_UPLOAD));

    const float x = ImGui::GetContentRegionAvail().x / 2;
    const float y = ImGui::GetFrameHeight();
    if (ui::button(load_str.c_str(), {x, y})) {
      FileDialogFilter dialog_filters[] = {{.name = "Asset (.oxasset)", .pattern = "oxasset"}};
      window.show_dialog({
          .kind = DialogKind::OpenFile,
          .user_data = &load_event,
          .callback =
              [](void* user_data, const c8* const* files, i32) {
        if (!files || !*files) {
          return;
        }

        const auto first_path_cstr = *files;
        const auto first_path_len = std::strlen(first_path_cstr);
        auto path = std::string(first_path_cstr, first_path_len);

        load_event_copy.emit<Event::DialogLoadEvent>({path});
      },
          .title = "Open material asset file...",
          .default_path = fs::current_path(),
          .filters = dialog_filters,
          .multi_select = false,
      });
    }
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ContentPanel::DRAG_DROP_SOURCE)) {
        const ::fs::path path = ui::get_path_from_imgui_payload(payload);
        if (const std::string ext = path.extension().string(); ext == ".oxasset") {
          load_event.emit<Event::DialogLoadEvent>({path});
        }
      }
      ImGui::EndDragDropTarget();
    }

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay)) {
      ImGui::BeginTooltip();
      ImGui::Text("You can drag&drop here to load a material.");
      ImGui::EndTooltip();
    }

    ImGui::SameLine();

    auto save_str = fmt::format("{} Save", StringUtils::from_char8_t(ICON_MDI_FILE_DOWNLOAD));
    if (ui::button(save_str.c_str(), {x, y})) {
      FileDialogFilter dialog_filters[] = {{.name = "Asset (.oxasset)", .pattern = "oxasset"}};
      window.show_dialog({
          .kind = DialogKind::SaveFile,
          .user_data = nullptr,
          .callback =
              [](void* user_data, const c8* const* files, i32) {
        if (!files || !*files || !uuid_copy) {
          return;
        }

        const auto first_path_cstr = *files;
        const auto first_path_len = std::strlen(first_path_cstr);
        auto path = std::string(first_path_cstr, first_path_len);

        load_event_copy.emit<Event::DialogSaveEvent>({path});
      },
          .title = "Open material asset file...",
          .default_path = fs::current_path(),
          .filters = dialog_filters,
          .multi_select = false,
      });
    }

    if (ImGui::BeginDragDropSource()) {
      std::string path_str = fmt::format("new_material");
      ContentPanel::PayloadData payload = {uuid, path_str};
      ImGui::SetDragDropPayload(ContentPanel::DRAG_DROP_TARGET, &payload, payload.size());
      ImGui::TextUnformatted(path_str.c_str());
      ImGui::EndDragDropSource();
    }

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay)) {
      ImGui::BeginTooltip();
      ImGui::Text("You can drag&drop this into content window to save the material.");
      ImGui::EndTooltip();
    }
  }

  ui::begin_properties(ui::default_properties_flags);

  const char* alpha_modes[] = {"Opaque", "Mash", "Blend"};
  ui::property("Alpha mode", reinterpret_cast<int*>(&material->alpha_mode), alpha_modes, 3);

  const char* samplers[] = {
      "LinearRepeated",
      "LinearClamped",
      "NearestRepeated",
      "NearestClamped",
      "LinearRepeatedAnisotropy",
  };
  ui::property("Sampler", reinterpret_cast<int*>(&material->sampling_mode), samplers, 5);

  ui::property_vector<glm::vec2>("UV Size", material->uv_size, 0.1f, 10.f);
  ui::property_vector<glm::vec2>("UV Offset", material->uv_offset, -10.0f, 10.f);

  ui::property_vector("Color", material->albedo_color, true, true);

  auto* asset_man = App::get_asset_manager();

  if (UUID new_asset = {}; ui::texture_property("Albedo", material->albedo_texture, &new_asset)) {
    asset_man->unload_texture(material->albedo_texture);
    material->albedo_texture = new_asset;
  }

  if (UUID new_asset = {}; ui::texture_property("Normal", material->normal_texture, &new_asset)) {
    asset_man->unload_texture(material->normal_texture);
    material->normal_texture = new_asset;
  }

  if (UUID new_asset = {}; ui::texture_property("Emissive", material->emissive_texture, &new_asset)) {
    asset_man->unload_texture(material->emissive_texture);
    material->emissive_texture = new_asset;
  }
  if (material->emissive_texture) {
    ui::property_vector("Emissive Color", material->emissive_color, true, true);
  }

  if (UUID new_asset = {};
      ui::texture_property("Metallic Roughness", material->metallic_roughness_texture, &new_asset)) {
    asset_man->unload_texture(material->metallic_roughness_texture);
    material->metallic_roughness_texture = new_asset;
  }
  if (material->metallic_roughness_texture) {
    ui::property("Roughness Factor", &material->roughness_factor, 0.0f, 1.0f);
    ui::property("Metallic Factor", &material->metallic_factor, 0.0f, 1.0f);
  }

  if (UUID new_asset = {}; ui::texture_property("Occlusion", material->occlusion_texture, &new_asset)) {
    asset_man->unload_texture(material->occlusion_texture);
    material->occlusion_texture = new_asset;
  }

  ui::end_properties();
}

template <typename T>
static void draw_particle_over_lifetime_module(const std::string_view module_name,
                                               OverLifetimeModule<T>& property_module,
                                               bool color = false,
                                               bool rotation = false) {
  static constexpr ImGuiTreeNodeFlags TREE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                                   ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed |
                                                   ImGuiTreeNodeFlags_FramePadding;

  if (ImGui::TreeNodeEx(module_name.data(), TREE_FLAGS, "%s", module_name.data())) {
    ui::begin_properties();
    ui::texture_property("Enabled", &property_module.enabled);

    if (rotation) {
      T degrees = glm::degrees(property_module.start);
      if (ui::property_vector("Start", degrees))
        property_module.start = glm::radians(degrees);

      degrees = glm::degrees(property_module.end);
      if (ui::property_vector("End", degrees))
        property_module.end = glm::radians(degrees);
    } else {
      ui::property_vector("Start", property_module.start, color);
      ui::property_vector("End", property_module.end, color);
    }

    ui::end_properties();

    ImGui::TreePop();
  }
}

template <typename T>
static void draw_particle_by_speed_module(const std::string_view module_name,
                                          BySpeedModule<T>& property_module,
                                          bool color = false,
                                          bool rotation = false) {
  static constexpr ImGuiTreeNodeFlags TREE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                                   ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed |
                                                   ImGuiTreeNodeFlags_FramePadding;

  if (ImGui::TreeNodeEx(module_name.data(), TREE_FLAGS, "%s", module_name.data())) {
    ui::begin_properties();
    ui::texture_property("Enabled", &property_module.enabled);

    if (rotation) {
      T degrees = glm::degrees(property_module.start);
      if (ui::property_vector("Start", degrees))
        property_module.start = glm::radians(degrees);

      degrees = glm::degrees(property_module.end);
      if (ui::property_vector("End", degrees))
        property_module.end = glm::radians(degrees);
    } else {
      ui::property_vector("Start", property_module.start, color);
      ui::property_vector("End", property_module.end, color);
    }

    ui::texture_property("Min Speed", &property_module.min_speed);
    ui::texture_property("Max Speed", &property_module.max_speed);
    ui::end_properties();
    ImGui::TreePop();
  }
}

template <typename Component>
void draw_add_component(const flecs::entity entity,
                        const char* name) {
  if (ImGui::MenuItem(name)) {
    if (entity.has<Component>())
      OX_LOG_WARN("Entity already has same component!");
    else
      entity.add<Component>();
    ImGui::CloseCurrentPopup();
  }
}

void InspectorPanel::draw_components(const flecs::entity entity) {
  ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.75f);
  static std::string new_name = {};
  if (_rename_entity)
    ImGui::SetKeyboardFocusHere();
  if (ui::input_text("##Tag", &new_name, ImGuiInputTextFlags_EnterReturnsTrue)) {
    entity.set_name(new_name.c_str());
  }
  ImGui::PopItemWidth();
  ImGui::SameLine();

  if (ui::button(StringUtils::from_char8_t(ICON_MDI_PLUS))) {
    ImGui::OpenPopup("Add Component");
  }
  if (ImGui::BeginPopup("Add Component")) {
    draw_add_component<MeshComponent>(selected_entity, "Mesh Renderer");
    draw_add_component<AudioSourceComponent>(selected_entity, "Audio Source");
    draw_add_component<AudioListenerComponent>(selected_entity, "Audio Listener");
    draw_add_component<LightComponent>(selected_entity, "Light");
    draw_add_component<ParticleSystemComponent>(selected_entity, "Particle System");
    draw_add_component<CameraComponent>(selected_entity, "Camera");
    draw_add_component<PostProcessProbe>(selected_entity, "PostProcess Probe");
    draw_add_component<RigidbodyComponent>(selected_entity, "Rigidbody");
    draw_add_component<BoxColliderComponent>(entity, "Box Collider");
    draw_add_component<SphereColliderComponent>(entity, "Sphere Collider");
    draw_add_component<CapsuleColliderComponent>(entity, "Capsule Collider");
    draw_add_component<TaperedCapsuleColliderComponent>(entity, "Tapered Capsule Collider");
    draw_add_component<CylinderColliderComponent>(entity, "Cylinder Collider");
    draw_add_component<MeshColliderComponent>(entity, "Mesh Collider");
    draw_add_component<CharacterControllerComponent>(entity, "Character Controller");
    draw_add_component<LuaScriptComponent>(entity, "Lua Script Component");
    draw_add_component<CPPScriptComponent>(entity, "CPP Script Component");
    draw_add_component<SpriteComponent>(entity, "Sprite Component");
    draw_add_component<SpriteAnimationComponent>(entity, "Sprite Animation Component");
    ImGui::EndPopup();
  }

  ImGui::SameLine();

  const auto uuidstr = fmt::format("EntityID: {}", entity.id());
  ImGui::TextUnformatted(uuidstr.c_str());

  draw_component<TransformComponent>(" Transform Component",
                                     entity,
                                     [](TransformComponent& component, flecs::entity e) {
    ui::begin_properties(ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV);
    ui::draw_vec3_control("Translation", component.position);
    glm::vec3 rotation = glm::degrees(component.rotation);
    ui::draw_vec3_control("Rotation", rotation);
    component.rotation = glm::radians(rotation);
    ui::draw_vec3_control("Scale", component.scale, nullptr, 1.0f);
    ui::end_properties();
  });

  draw_component<MeshComponent>(" Mesh Component", entity, [](MeshComponent& component, flecs::entity e) {

  });

  draw_component<SpriteComponent>(" Sprite Component", entity, [](SpriteComponent& component, flecs::entity e) {
    ui::begin_properties();
    ui::property("Layer", &component.layer);
    ui::property("SortY", &component.sort_y);
    ui::property("FlipX", &component.flip_x);
    ui::end_properties();

    ImGui::SeparatorText("Material");

    auto load_event = App::get()->world.entity("sprite_material_load_event");
    load_event.observe<Event::DialogLoadEvent>([&component](Event::DialogLoadEvent& e) {
      auto* asset_man = App::get_asset_manager();
      if (auto imported = asset_man->import_asset(e.path)) {
        component.material = imported;
        asset_man->unload_asset(component.material);
      }
    });

    load_event.observe<Event::DialogSaveEvent>([&component](Event::DialogSaveEvent& e) {
      auto* asset_man = App::get_asset_manager();
      asset_man->export_asset(component.material, e.path);
    });

    if (auto* material = App::get_asset_manager()->get_material(component.material)) {
      draw_material_properties(material, component.material, load_event);
    }
  });

  draw_component<SpriteAnimationComponent>(" Sprite Animation Component",
                                           entity,
                                           [this](SpriteAnimationComponent& component, flecs::entity e) {
    ui::begin_properties();
    if (ui::property("Number of frames", &component.num_frames))
      component.reset();
    if (ui::property("Loop", &component.loop))
      component.reset();
    if (ui::property("Inverted", &component.inverted))
      component.reset();
    if (ui::property("Frames per second", &component.fps))
      component.reset();
    if (ui::property("Columns", &component.columns))
      component.reset();
    if (ui::draw_vec2_control("Frame size", component.frame_size))
      component.reset();
    const float x = ImGui::GetContentRegionAvail().x;
    const float y = ImGui::GetFrameHeight();
    ImGui::Spacing();
    if (ui::button("Auto", {x, y})) {
      if (const auto* sc = e.get<SpriteComponent>()) {
        auto asset_man = App::get_asset_manager();
        if (const auto* material = asset_man->get_material(sc->material)) {
          if (const auto* texture = asset_man->get_texture(material->albedo_texture)) {
            component.set_frame_size(texture->get_extent().width, texture->get_extent().height);
          }
        }
      }
    }
    ui::end_properties();
  });

  draw_component<PostProcessProbe>(" PostProcess Probe Component",
                                   entity,
                                   [](PostProcessProbe& component, flecs::entity e) {
    ImGui::Text("Vignette");
    ui::begin_properties();
    ui::property("Enable", &component.vignette_enabled);
    ui::property("Intensity", &component.vignette_intensity);
    ui::end_properties();
    ImGui::Separator();

    ImGui::Text("FilmGrain");
    ui::begin_properties();
    ui::property("Enable", &component.film_grain_enabled);
    ui::property("Intensity", &component.film_grain_intensity);
    ui::end_properties();
    ImGui::Separator();

    ImGui::Text("ChromaticAberration");
    ui::begin_properties();
    ui::property("Enable", &component.chromatic_aberration_enabled);
    ui::property("Intensity", &component.chromatic_aberration_intensity);
    ui::end_properties();
    ImGui::Separator();

    ImGui::Text("Sharpen");
    ui::begin_properties();
    ui::property("Enable", &component.sharpen_enabled);
    ui::property("Intensity", &component.sharpen_intensity);
    ui::end_properties();
    ImGui::Separator();
  });

  draw_component<AudioSourceComponent>(" Audio Source Component",
                                       entity,
                                       [&entity, this](AudioSourceComponent& component, flecs::entity e) {
    auto* asset_man = App::get_asset_manager();
    auto* asset = asset_man->get_asset(component.audio_source);

    const std::string filepath =
        (asset && asset->is_loaded())
            ? asset->path
            : fmt::format("{} Drop an audio file", StringUtils::from_char8_t(ICON_MDI_FILE_UPLOAD));

    const float x = ImGui::GetContentRegionAvail().x;
    const float y = ImGui::GetFrameHeight();
    if (ui::button(filepath.c_str(), {x, y})) {
      const auto& window = App::get()->get_window();
      FileDialogFilter dialog_filters[] = {{.name = "Audio file(.mp3, .wav, .flac)", .pattern = "mp3;wav;flac"}};
      window.show_dialog({
          .kind = DialogKind::OpenFile,
          .user_data = &component,
          .callback =
              [](void* user_data, const c8* const* files, i32) {
        auto* comp = static_cast<AudioSourceComponent*>(user_data);

        if (!files || !*files) {
          return;
        }

        const auto first_path_cstr = *files;
        const auto first_path_len = std::strlen(first_path_cstr);
        auto path = ::fs::path(std::string(first_path_cstr, first_path_len));
        if (const std::string ext = path.extension().string(); ext == ".mp3" || ext == ".wav" || ext == ".flac") {
          auto* am = App::get_asset_manager();
          comp->audio_source = am->create_asset(AssetType::Audio, path.string());
          am->load_asset(comp->audio_source);
        }
      },
          .title = "Open audio file...",
          .default_path = fs::current_path(),
          .filters = dialog_filters,
          .multi_select = false,
      });
    }
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
        const std::filesystem::path path = ui::get_path_from_imgui_payload(payload);
        if (const std::string ext = path.extension().string(); ext == ".mp3" || ext == ".wav" || ext == ".flac") {
          component.audio_source = asset_man->create_asset(AssetType::Audio, path.string());
          asset_man->load_asset(component.audio_source);
        }
      }
      ImGui::EndDragDropTarget();
    }
    ImGui::Spacing();

    auto* audio_asset = asset_man->get_audio(component.audio_source);
    if (!audio_asset)
      return;

    auto* audio_engine = App::get_system<AudioEngine>(EngineSystems::AudioEngine);

    ui::begin_properties();
    if (ui::property("Volume Multiplier", &component.volume))
      audio_engine->set_source_volume(audio_asset->get_source(), component.volume);
    if (ui::property("Pitch Multiplier", &component.pitch))
      audio_engine->set_source_pitch(audio_asset->get_source(), component.pitch);
    if (ui::property("Looping", &component.looping))
      audio_engine->set_source_looping(audio_asset->get_source(), component.looping);
    ui::property("Play On Awake", &component.play_on_awake);
    ui::end_properties();

    ImGui::Spacing();
    if (ui::button(StringUtils::from_char8_t(ICON_MDI_PLAY "Play ")))
      audio_engine->play_source(audio_asset->get_source());
    ImGui::SameLine();
    if (ui::button(StringUtils::from_char8_t(ICON_MDI_PAUSE "Pause ")))
      audio_engine->pause_source(audio_asset->get_source());
    ImGui::SameLine();
    if (ui::button(StringUtils::from_char8_t(ICON_MDI_STOP "Stop ")))
      audio_engine->stop_source(audio_asset->get_source());
    ImGui::Spacing();

    ui::begin_properties();
    ui::property("Spatialization", &component.spatialization);

    if (component.spatialization) {
      ImGui::Indent();
      const char* attenuation_type_strings[] = {"None", "Inverse", "Linear", "Exponential"};
      int attenuation_type = static_cast<int>(component.attenuation_model);
      if (ui::property("Attenuation Model", &attenuation_type, attenuation_type_strings, 4)) {
        component.attenuation_model = static_cast<AttenuationModelType>(attenuation_type);
        audio_engine->set_source_attenuation_model(audio_asset->get_source(), component.attenuation_model);
      }
      if (ui::property("Roll Off", &component.roll_off))
        audio_engine->set_source_roll_off(audio_asset->get_source(), component.roll_off);
      if (ui::property("Min Gain", &component.min_gain))
        audio_engine->set_source_min_gain(audio_asset->get_source(), component.min_gain);
      if (ui::property("Max Gain", &component.max_gain))
        audio_engine->set_source_max_gain(audio_asset->get_source(), component.max_gain);
      if (ui::property("Min Distance", &component.min_distance))
        audio_engine->set_source_min_distance(audio_asset->get_source(), component.min_distance);
      if (ui::property("Max Distance", &component.max_distance))
        audio_engine->set_source_max_distance(audio_asset->get_source(), component.max_distance);
      float degrees = glm::degrees(component.cone_inner_angle);
      if (ui::property("Cone Inner Angle", &degrees))
        component.cone_inner_angle = glm::radians(degrees);
      degrees = glm::degrees(component.cone_outer_angle);
      if (ui::property("Cone Outer Angle", &degrees))
        component.cone_outer_angle = glm::radians(degrees);
      ui::property("Cone Outer Gain", &component.cone_outer_gain);
      ui::property("Doppler Factor", &component.doppler_factor);
      audio_engine->set_source_cone(audio_asset->get_source(),
                                    component.cone_inner_angle,
                                    component.cone_outer_angle,
                                    component.cone_outer_gain);
      ImGui::Unindent();
    }
    ui::end_properties();
  });

  draw_component<AudioListenerComponent>(" Audio Listener Component",
                                         entity,
                                         [](AudioListenerComponent& component, flecs::entity e) {
    ui::begin_properties();
    ui::property("Active", &component.active);
    float degrees = glm::degrees(component.cone_inner_angle);
    if (ui::property("Cone Inner Angle", &degrees))
      component.cone_inner_angle = glm::radians(degrees);
    degrees = glm::degrees(component.cone_outer_angle);
    if (ui::property("Cone Outer Angle", &degrees))
      component.cone_outer_angle = glm::radians(degrees);
    ui::property("Cone Outer Gain", &component.cone_outer_gain);

    auto* audio_engine = App::get_system<AudioEngine>(EngineSystems::AudioEngine);
    audio_engine->set_listener_cone(component.listener_index,
                                    component.cone_inner_angle,
                                    component.cone_outer_angle,
                                    component.cone_outer_gain);
    ui::end_properties();
  });

  draw_component<LightComponent>(" Light Component", entity, [](LightComponent& component, flecs::entity e) {
    ui::begin_properties();
    const char* light_type_strings[] = {"Directional", "Point", "Spot"};
    int light_type = component.type;
    if (ui::property("Light Type", &light_type, light_type_strings, 3))
      component.type = static_cast<LightComponent::LightType>(light_type);

    if (ui::property("Color Temperature Mode", &component.color_temperature_mode) && component.color_temperature_mode) {
      ColorUtils::TempratureToColor(component.temperature, component.color);
    }

    if (component.color_temperature_mode) {
      if (ui::property<u32>("Temperature (K)", &component.temperature, 1000, 40000))
        ColorUtils::TempratureToColor(component.temperature, component.color);
    } else {
      ui::property_vector("Color", component.color, true);
    }

    ui::property("Intensity", &component.intensity, 0.0f, 100.0f);

    if (component.type != LightComponent::Directional) {
      ui::property("Range", &component.range, 0.0f, 100.0f);
      ui::property("Radius", &component.radius, 0.0f, 100.0f);
      ui::property("Length", &component.length, 0.0f, 100.0f);
    }

    if (component.type == LightComponent::Spot) {
      ui::property("Outer Cone Angle", &component.outer_cone_angle, 0.0f, 100.0f);
      ui::property("Inner Cone Angle", &component.inner_cone_angle, 0.0f, 100.0f);
    }

    ui::property("Cast Shadows", &component.cast_shadows);

    const ankerl::unordered_dense::map<u32, int> res_map = {{0, 0}, {512, 1}, {1024, 2}, {2048, 3}};
    const ankerl::unordered_dense::map<int, u32> id_map = {{0, 0}, {1, 512}, {2, 1024}, {3, 2048}};

    const char* res_strings[] = {"Auto", "512", "1024", "2048"};
    int idx = res_map.at(component.shadow_map_res);
    if (ui::property("Shadow Resolution", &idx, res_strings, 4)) {
      component.shadow_map_res = id_map.at(idx);
    }

    if (component.type == LightComponent::Directional) {
      for (u32 i = 0; i < (u32)component.cascade_distances.size(); ++i)
        ui::property(fmt::format("Cascade {}", i).c_str(), &component.cascade_distances[i]);
    }

    ui::end_properties();
  });

  draw_component<RigidbodyComponent>(" Rigidbody Component",
                                     entity,
                                     [](RigidbodyComponent& component, flecs::entity e) {
    ui::begin_properties();

    const char* dofs_strings[] = {
        "None",
        "All",
        "Plane2D",
        "Custom",
    };
    int current_dof_selection = 3;
    switch (component.allowed_dofs) {
      case RigidbodyComponent::AllowedDOFs::None   : current_dof_selection = 0; break;
      case RigidbodyComponent::AllowedDOFs::All    : current_dof_selection = 1; break;
      case RigidbodyComponent::AllowedDOFs::Plane2D: current_dof_selection = 2; break;
      default                                      : current_dof_selection = 3; break;
    }

    if (ui::property("Allowed degree of freedom", &current_dof_selection, dofs_strings, std::size(dofs_strings))) {
      switch (current_dof_selection) {
        case 0: component.allowed_dofs = RigidbodyComponent::AllowedDOFs::None; break;
        case 1: component.allowed_dofs = RigidbodyComponent::AllowedDOFs::All; break;
        case 2: component.allowed_dofs = RigidbodyComponent::AllowedDOFs::Plane2D; break;
      }
    }

    ImGui::Indent();
    ui::begin_property_grid("Allowed positions", nullptr);
    ImGui::CheckboxFlags("x", (u32*)&component.allowed_dofs, (u32)RigidbodyComponent::AllowedDOFs::TranslationX);
    ImGui::SameLine();
    ImGui::CheckboxFlags("y", (u32*)&component.allowed_dofs, (u32)RigidbodyComponent::AllowedDOFs::TranslationY);
    ImGui::SameLine();
    ImGui::CheckboxFlags("z", (u32*)&component.allowed_dofs, (u32)RigidbodyComponent::AllowedDOFs::TranslationZ);
    ui::end_property_grid();

    ui::begin_property_grid("Allowed rotations", nullptr);
    ImGui::CheckboxFlags("x", (u32*)&component.allowed_dofs, (u32)RigidbodyComponent::AllowedDOFs::RotationX);
    ImGui::SameLine();
    ImGui::CheckboxFlags("y", (u32*)&component.allowed_dofs, (u32)RigidbodyComponent::AllowedDOFs::RotationY);
    ImGui::SameLine();
    ImGui::CheckboxFlags("z", (u32*)&component.allowed_dofs, (u32)RigidbodyComponent::AllowedDOFs::RotationZ);
    ui::end_property_grid();
    ImGui::Unindent();

    const char* body_type_strings[] = {"Static", "Kinematic", "Dynamic"};
    int body_type = static_cast<int>(component.type);
    if (ui::property("Body Type", &body_type, body_type_strings, 3))
      component.type = static_cast<RigidbodyComponent::BodyType>(body_type);

    ui::property("Allow Sleep", &component.allow_sleep);
    ui::property("Awake", &component.awake);
    if (component.type == RigidbodyComponent::BodyType::Dynamic) {
      ui::property("Mass", &component.mass, 0.01f, 10000.0f);
      ui::property("Linear Drag", &component.linear_drag);
      ui::property("Angular Drag", &component.angular_drag);
      ui::property("Gravity Scale", &component.gravity_scale);
      ui::property("Continuous", &component.continuous);
      ui::property("Interpolation", &component.interpolation);

      component.linear_drag = glm::max(component.linear_drag, 0.0f);
      component.angular_drag = glm::max(component.angular_drag, 0.0f);
    }

    ui::property("Is Sensor", &component.is_sensor);
    ui::end_properties();
  });

  draw_component<BoxColliderComponent>(" Box Collider", entity, [](BoxColliderComponent& component, flecs::entity e) {
    ui::begin_properties();
    ui::property_vector("Size", component.size);
    ui::property_vector("Offset", component.offset);
    ui::property("Density", &component.density);
    ui::property("Friction", &component.friction, 0.0f, 1.0f);
    ui::property("Restitution", &component.restitution, 0.0f, 1.0f);
    ui::end_properties();

    component.density = glm::max(component.density, 0.001f);
  });

  draw_component<SphereColliderComponent>(" Sphere Collider",
                                          entity,
                                          [](SphereColliderComponent& component, flecs::entity e) {
    ui::begin_properties();
    ui::property("Radius", &component.radius);
    ui::property_vector("Offset", component.offset);
    ui::property("Density", &component.density);
    ui::property("Friction", &component.friction, 0.0f, 1.0f);
    ui::property("Restitution", &component.restitution, 0.0f, 1.0f);
    ui::end_properties();

    component.density = glm::max(component.density, 0.001f);
  });

  draw_component<CapsuleColliderComponent>(" Capsule Collider",
                                           entity,
                                           [](CapsuleColliderComponent& component, flecs::entity e) {
    ui::begin_properties();
    ui::property("Height", &component.height);
    ui::property("Radius", &component.radius);
    ui::property_vector("Offset", component.offset);
    ui::property("Density", &component.density);
    ui::property("Friction", &component.friction, 0.0f, 1.0f);
    ui::property("Restitution", &component.restitution, 0.0f, 1.0f);
    ui::end_properties();

    component.density = glm::max(component.density, 0.001f);
  });

  draw_component<TaperedCapsuleColliderComponent>(" Tapered Capsule Collider",
                                                  entity,
                                                  [](TaperedCapsuleColliderComponent& component, flecs::entity e) {
    ui::begin_properties();
    ui::property("Height", &component.height);
    ui::property("Top Radius", &component.top_radius);
    ui::property("Bottom Radius", &component.bottom_radius);
    ui::property_vector("Offset", component.offset);
    ui::property("Density", &component.density);
    ui::property("Friction", &component.friction, 0.0f, 1.0f);
    ui::property("Restitution", &component.restitution, 0.0f, 1.0f);
    ui::end_properties();

    component.density = glm::max(component.density, 0.001f);
  });

  draw_component<CylinderColliderComponent>(" Cylinder Collider",
                                            entity,
                                            [](CylinderColliderComponent& component, flecs::entity e) {
    ui::begin_properties();
    ui::property("Height", &component.height);
    ui::property("Radius", &component.radius);
    ui::property_vector("Offset", component.offset);
    ui::property("Density", &component.density);
    ui::property("Friction", &component.friction, 0.0f, 1.0f);
    ui::property("Restitution", &component.restitution, 0.0f, 1.0f);
    ui::end_properties();

    component.density = glm::max(component.density, 0.001f);
  });

  draw_component<MeshColliderComponent>(" Mesh Collider",
                                        entity,
                                        [](MeshColliderComponent& component, flecs::entity e) {
    ui::begin_properties();
    ui::property_vector("Offset", component.offset);
    ui::property("Friction", &component.friction, 0.0f, 1.0f);
    ui::property("Restitution", &component.restitution, 0.0f, 1.0f);
    ui::end_properties();
  });

  draw_component<CharacterControllerComponent>(" Character Controller",
                                               entity,
                                               [](CharacterControllerComponent& component, flecs::entity e) {
    ui::begin_properties();
    ui::property("CharacterHeightStanding", &component.character_height_standing);
    ui::property("CharacterRadiusStanding", &component.character_radius_standing);
    ui::property("CharacterHeightCrouching", &component.character_height_crouching);
    ui::property("CharacterRadiusCrouching", &component.character_radius_crouching);

    // Movement
    ui::property("ControlMovementDuringJump", &component.control_movement_during_jump);
    ui::property("JumpForce", &component.jump_force);

    ui::property("Friction", &component.friction, 0.0f, 1.0f);
    ui::property("CollisionTolerance", &component.collision_tolerance);
    ui::end_properties();
  });

  draw_component<CameraComponent>("Camera Component", entity, [](CameraComponent& component, flecs::entity e) {
    const auto is_perspective = component.projection == CameraComponent::Projection::Perspective;
    ui::begin_properties();

    const char* proj_strs[] = {"Perspective", "Orthographic"};
    int proj = static_cast<int>(component.projection);
    if (ui::property("Projection", &proj, proj_strs, 2))
      component.projection = static_cast<CameraComponent::Projection>(proj);

    if (is_perspective) {
      ui::property("FOV", &component.fov);
      ui::property("Near Clip", &component.near_clip);
      ui::property("Far Clip", &component.far_clip);
    } else {
      ui::property("Zoom", &component.zoom);
    }

    ui::end_properties();
  });

  draw_component<LuaScriptComponent>("Lua Script Component",
                                     entity,
                                     [](LuaScriptComponent& component, flecs::entity e) {
    const float filter_cursor_pos_x = ImGui::GetCursorPosX();
    ImGuiTextFilter name_filter;

    name_filter.Draw("##scripts_filter",
                     ImGui::GetContentRegionAvail().x -
                         (ui::get_icon_button_size(ICON_MDI_PLUS, "").x + 2.0f * ImGui::GetStyle().FramePadding.x));

    if (!name_filter.IsActive()) {
      ImGui::SameLine();
      ImGui::SetCursorPosX(filter_cursor_pos_x + ImGui::GetFontSize() * 0.5f);
      ImGui::TextUnformatted(StringUtils::from_char8_t(ICON_MDI_MAGNIFY " Search..."));
    }

    for (u32 i = 0; i < (u32)component.lua_systems.size(); i++) {
      const auto& system = component.lua_systems[i];
      auto name = fs::get_file_name(system->get_path());
      if (name_filter.PassFilter(name.c_str())) {
        ImGui::PushID(i);
        constexpr ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;
        if (ImGui::TreeNodeEx(name.c_str(), flags, "%s", name.c_str())) {
          auto rld_str = fmt::format("{} Reload", StringUtils::from_char8_t(ICON_MDI_REFRESH));
          if (ui::button(rld_str.c_str()))
            system->reload();
          ImGui::SameLine();
          auto rmv_str = fmt::format("{} Remove", StringUtils::from_char8_t(ICON_MDI_TRASH_CAN));
          if (ui::button(rmv_str.c_str()))
            component.lua_systems.erase(component.lua_systems.begin() + i);
          ImGui::TreePop();
        }
        ImGui::PopID();
      }
    }

    const float x = ImGui::GetContentRegionAvail().x;
    const float y = ImGui::GetFrameHeight();
    const auto btn = fmt::format("{} Drop a script file", StringUtils::from_char8_t(ICON_MDI_FILE_UPLOAD));
    if (ui::button(btn.c_str(), {x, y})) {
      const auto& window = App::get()->get_window();
      FileDialogFilter dialog_filters[] = {{.name = "Lua file(.lua)", .pattern = "lua"}};
      window.show_dialog({
          .kind = DialogKind::OpenFile,
          .user_data = &component,
          .callback =
              [](void* user_data, const c8* const* files, i32) {
        auto* user_data_comp = static_cast<LuaScriptComponent*>(user_data);
        if (!files || !*files) {
          return;
        }

        const auto first_path_cstr = *files;
        const auto first_path_len = std::strlen(first_path_cstr);
        const auto p_str = std::string(first_path_cstr, first_path_len);
        if (fs::get_file_extension(p_str) == "lua") {
          user_data_comp->lua_systems.emplace_back(create_shared<LuaSystem>(p_str));
        }
      },
          .title = "Open lua file...",
          .default_path = fs::current_path(),
          .filters = dialog_filters,
          .multi_select = false,
      });
    }
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
        const auto path = ui::get_path_from_imgui_payload(payload);
        if (path.empty())
          return;
        if (fs::get_file_extension(path) == "lua") {
          component.lua_systems.emplace_back(create_shared<LuaSystem>(path));
        }
      }
      ImGui::EndDragDropTarget();
    }
  });

  draw_component<CPPScriptComponent>(" CPP Script Component",
                                     entity,
                                     [](CPPScriptComponent& component, flecs::entity e) {
    const auto lbl = fmt::format("{} Add system", StringUtils::from_char8_t(ICON_MDI_PLUS_OUTLINE));
    ankerl::unordered_dense::map<size_t, std::string> system_names_strs = {};
    std::vector<const char*> system_names = {};
    system_names.emplace_back(lbl.c_str());

    std::vector<size_t> system_hashes = {};
    system_hashes.emplace_back(0);

    int current_system_selection = 0;

    auto* system_manager = App::get_system<SystemManager>(EngineSystems::SystemManager);
    for (auto& [hash, pair] : system_manager->system_registry) {
      auto& [name, system] = pair;
      std::string system_name = name;
      system_name = system_name.erase(0, system_name.find(' ') + 1);
      system_names.emplace_back(system_names_strs.emplace(hash, system_name).first->second.c_str());
      system_hashes.emplace_back(hash);
    }

    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("table", 1, flags)) {
      ImGui::TableSetupColumn("  Systems", ImGuiTableColumnFlags_NoHide);
      ImGui::TableHeadersRow();

      i32 delete_index = -1;

      for (u32 index = 0; auto& sys : component.systems) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushID(index);
        if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_TRASH_CAN)))
          delete_index = index;
        ImGui::PopID();
        ImGui::SameLine();
        bool open = ImGui::TreeNodeEx(system_names_strs[sys->hash_code].c_str(), ImGuiTreeNodeFlags_SpanAllColumns);
        if (open) {
          ImGui::Indent();
          ImGui::Text("system_hash: %s", std::to_string(sys->hash_code).c_str());
          ImGui::Unindent();
          ImGui::TreePop();
        }

        index += 1;
      }

      if (delete_index > -1)
        component.systems.erase(component.systems.begin() + delete_index);

      ImGui::EndTable();
    }

    const float x = ImGui::GetContentRegionAvail().x;
    ImGui::PushItemWidth(x);
    if (ui::combo(lbl.c_str(),
                  &current_system_selection,
                  system_names.data(),
                  static_cast<i32>(system_names.size()),
                  std::to_string(system_hashes[current_system_selection]).c_str())) {
      if (auto system = system_manager->get_system(system_hashes[current_system_selection]))
        component.systems.emplace_back(system);
    }
  });

  draw_component<ParticleSystemComponent>("Particle System Component",
                                          entity,
                                          [](const ParticleSystemComponent& component, flecs::entity e) {
#if 0
    auto& props = component.system->get_properties();

    ImGui::Text("Active Particles Count: %u", component.system->get_active_particle_count());
    ImGui::BeginDisabled(props.looping);
    if (ui::button(StringUtils::from_char8_t(ICON_MDI_PLAY)))
      component.system->play();
    ImGui::SameLine();
    if (ui::button(StringUtils::from_char8_t(ICON_MDI_STOP)))
      component.system->stop();
    ImGui::EndDisabled();

    ImGui::Separator();

    ui::begin_properties();
    ui::texture_property("Duration", &props.duration);
    if (ui::texture_property("Looping", &props.looping)) {
      if (props.looping)
        component.system->play();
    }
    ui::texture_property("Start Delay", &props.start_delay);
    ui::texture_property("Start Lifetime", &props.start_lifetime);
    ui::property_vector("Start Velocity", props.start_velocity);
    ui::property_vector("Start Color", props.start_color, true);
    ui::property_vector("Start Size", props.start_size);
    ui::property_vector("Start Rotation", props.start_rotation);
    ui::texture_property("Gravity Modifier", &props.gravity_modifier);
    ui::texture_property("Simulation Speed", &props.simulation_speed);
    ui::texture_property("Play On Awake", &props.play_on_awake);
    ui::texture_property("Max Particles", &props.max_particles);
    ui::end_properties();

    ImGui::Separator();

    ui::begin_properties();
    ui::texture_property("Rate Over Time", &props.rate_over_time);
    ui::texture_property("Rate Over Distance", &props.rate_over_distance);
    ui::texture_property("Burst Count", &props.burst_count);
    ui::texture_property("Burst Time", &props.burst_time);
    ui::property_vector("Position Start", props.position_start);
    ui::property_vector("Position End", props.position_end);
    // OxUI::Property("Texture", props.Texture); //TODO:
    ui::end_properties();

    draw_particle_over_lifetime_module("Velocity Over Lifetime", props.velocity_over_lifetime);
    draw_particle_over_lifetime_module("Force Over Lifetime", props.force_over_lifetime);
    draw_particle_over_lifetime_module("Color Over Lifetime", props.color_over_lifetime, true);
    draw_particle_by_speed_module("Color By Speed", props.color_by_speed, true);
    draw_particle_over_lifetime_module("Size Over Lifetime", props.size_over_lifetime);
    draw_particle_by_speed_module("Size By Speed", props.size_by_speed);
    draw_particle_over_lifetime_module("Rotation Over Lifetime", props.rotation_over_lifetime, false, true);
    draw_particle_by_speed_module("Rotation By Speed", props.rotation_by_speed, false, true);
#endif
  });
}
} // namespace ox
