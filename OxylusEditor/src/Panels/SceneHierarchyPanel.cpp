#include "SceneHierarchyPanel.hpp"

#include <glm/trigonometric.hpp>
#include <icons/IconsMaterialDesignIcons.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Asset/AssetFile.hpp"
#include "Asset/AssetManager.hpp"
#include "Asset/Material.hpp"
#include "Core/VFS.hpp"
#include "EditorLayer.hpp"
#include "Scene/ECSModule/Core.hpp"
#include "UI/ImGuiLayer.hpp"
#include "UI/OxUI.hpp"
#include "Utils/ImGuiScoped.hpp"
#include "Utils/StringUtils.hpp"

namespace ox {
SceneHierarchyPanel::SceneHierarchyPanel() :
    EditorPanel("Scene Hierarchy",
                ICON_MDI_VIEW_LIST,
                true) {}

auto SceneHierarchyPanel::get_selected_entity() const -> flecs::entity { return _selected_entity; }

auto SceneHierarchyPanel::set_selected_entity(flecs::entity entity) -> void { _selected_entity = entity; }

auto SceneHierarchyPanel::set_scene(const Shared<Scene>& scene) -> void {
  _scene = scene;
  _selected_entity = flecs::entity::null();
}

auto SceneHierarchyPanel::draw_entity_node(flecs::entity entity,
                                           uint32_t depth,
                                           bool force_expand_tree,
                                           bool is_part_of_prefab) -> ImRect {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();

  const auto child_count = _scene->world.count(flecs::ChildOf, entity);

  if (_filter.IsActive() && !_filter.PassFilter(entity.name().c_str())) {
    entity.children([this](flecs::entity child) { draw_entity_node(child); });
    return {0, 0, 0, 0};
  }

  const auto is_selected = _selected_entity.id() == entity.id();

  ImGuiTreeNodeFlags flags = (is_selected ? ImGuiTreeNodeFlags_Selected : 0);
  flags |= ImGuiTreeNodeFlags_OpenOnArrow;
  flags |= ImGuiTreeNodeFlags_SpanFullWidth;
  flags |= ImGuiTreeNodeFlags_FramePadding;

  if (child_count == 0) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }

  const bool highlight = is_selected;
  const auto header_selected_color = ImGuiLayer::header_selected_color;
  const auto popup_item_spacing = ImGuiLayer::popup_item_spacing;
  if (highlight) {
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(header_selected_color));
    ImGui::PushStyleColor(ImGuiCol_Header, header_selected_color);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, header_selected_color);
  }

  if (force_expand_tree)
    ImGui::SetNextItemOpen(true);

  const bool prefab_color_applied = is_part_of_prefab && !is_selected;
  if (prefab_color_applied)
    ImGui::PushStyleColor(ImGuiCol_Text, header_selected_color);

  const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void*>(entity.raw_id()),
                                        flags,
                                        "%s %s",
                                        StringUtils::from_char8_t(ICON_MDI_CUBE_OUTLINE),
                                        entity.name().c_str());

  if (highlight)
    ImGui::PopStyleColor(2);

  // Select
  if (!ImGui::IsItemToggledOpen() && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
    _selected_entity = entity;
  }

  // Expand recursively
  if (ImGui::IsItemToggledOpen() && (ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt)))
    force_expand_tree = opened;

  bool entity_deleted = false;

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, popup_item_spacing);
  if (ImGui::BeginPopupContextItem()) {
    if (ImGui::MenuItem("Rename", "F2"))
      _renaming_entity = entity;
    if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
      _selected_entity = entity.clone(true);
    if (ImGui::MenuItem("Delete", "Del"))
      entity_deleted = true;

    ImGui::Separator();

    draw_context_menu();

    ImGui::EndPopup();
  }
  ImGui::PopStyleVar();

  ImVec2 vertical_line_start = ImGui::GetCursorScreenPos();
  vertical_line_start.x -= 0.5f;
  vertical_line_start.y -= ImGui::GetFrameHeight() * 0.5f;

  // Drag Drop
  {
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* entity_payload = ImGui::AcceptDragDropPayload("Entity")) {
        _dragged_entity = *static_cast<flecs::entity*>(entity_payload->Data);
        _dragged_entity_target = entity;
      } else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
        const std::filesystem::path path = std::filesystem::path((const char*)payload->Data);
        if (path.extension() == ".oxprefab") {
          // dragged_entity = EntitySerializer::deserialize_entity_as_prefab(path.string().c_str(), _scene.get());
          // dragged_entity = entity;
        }
      }

      ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginDragDropSource()) {
      ImGui::SetDragDropPayload("Entity", &entity, sizeof(flecs::entity));
      ImGui::TextUnformatted(entity.name().c_str());
      ImGui::EndDragDropSource();
    }
  }

  if (entity.id() == _renaming_entity.id()) {
    static bool renaming = false;
    if (!renaming) {
      renaming = true;
      ImGui::SetKeyboardFocusHere();
    }

    std::string name{entity.name()};
    if (ImGui::InputText("##Tag", &name)) {
      entity.set_name(name.c_str());
    }

    if (ImGui::IsItemDeactivated()) {
      renaming = false;
      _renaming_entity = flecs::entity::null();
    }
  }

  ImGui::TableNextColumn();

  ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0, 0, 0, 0});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0, 0, 0, 0});

  const float button_size_x = ImGui::GetContentRegionAvail().x;
  const float frame_height = ImGui::GetFrameHeight();
  ImGui::PushID(entity.name());
  ImGui::Button(is_part_of_prefab ? "Prefab" : "Entity", {button_size_x, frame_height});
  ImGui::PopID();
  // Select
  if (ImGui::IsItemDeactivated() && ImGui::IsItemHovered() && !ImGui::IsItemToggledOpen()) {
    _selected_entity = entity;
  }

  ImGui::TableNextColumn();
  // Visibility Toggle
  {
    ImGui::Text("  %s",
                reinterpret_cast<const char*>(entity.enabled() ? ICON_MDI_EYE_OUTLINE : ICON_MDI_EYE_OFF_OUTLINE));

    if (ImGui::IsItemHovered() && (ImGui::IsMouseDragging(0) || ImGui::IsItemClicked())) {
      entity.enabled() ? entity.disable() : entity.enable();
    }
  }

  ImGui::PopStyleColor(3);

  if (prefab_color_applied)
    ImGui::PopStyleColor();

  // Open
  const ImRect node_rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
  {
    if (opened && !entity_deleted) {
      ImColor tree_line_color;
      depth %= 4;
      switch (depth) {
        case 0 : tree_line_color = ImColor(254, 112, 246); break;
        case 1 : tree_line_color = ImColor(142, 112, 254); break;
        case 2 : tree_line_color = ImColor(112, 180, 254); break;
        case 3 : tree_line_color = ImColor(48, 134, 198); break;
        default: tree_line_color = ImColor(255, 255, 255); break;
      }

      entity.children([this, depth, force_expand_tree, is_part_of_prefab, vertical_line_start, tree_line_color](
                          const flecs::entity child) {
        const float horizontal_tree_line_size = _scene->world.count(flecs::ChildOf, child) > 0 ? 9.f : 18.f;
        // chosen arbitrarily
        const ImRect child_rect = draw_entity_node(child, depth + 1, force_expand_tree, is_part_of_prefab);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 vertical_line_end = vertical_line_start;
        constexpr float line_thickness = 1.5f;
        const float midpoint = (child_rect.Min.y + child_rect.Max.y) / 2.0f;
        draw_list->AddLine(ImVec2(vertical_line_start.x, midpoint),
                           ImVec2(vertical_line_start.x + horizontal_tree_line_size, midpoint),
                           tree_line_color,
                           line_thickness);
        vertical_line_end.y = midpoint;
        draw_list->AddLine(vertical_line_start, vertical_line_end, tree_line_color, line_thickness);
      });
    }

    if (opened && child_count > 0)
      ImGui::TreePop();
  }

  // PostProcess Actions
  if (entity_deleted)
    _deleted_entity = entity;

  return node_rect;
}

void SceneHierarchyPanel::drag_drop_target() const {
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
      const std::filesystem::path path = ui::get_path_from_imgui_payload(payload);
      if (path.extension() == ".oxscene") {
        EditorLayer::get()->open_scene(path);
      }
      if (path.extension() == ".gltf" || path.extension() == ".glb") {
        // const auto mesh = AssetManager::get_mesh_asset(path.string());
        // _scene->load_mesh(mesh);
        // const auto mesh_task = AssetManager::get_mesh_asset_future(path.string());
        // context->trigger_future_mesh_load_event(FutureMeshLoadEvent{fs::get_name_with_extension(path.string()),
        // mesh_task});
      }
      if (path.extension() == ".oxprefab") {
        // EntitySerializer::deserialize_entity_as_prefab(path.string().c_str(), _scene.get());
      }
    }

    ImGui::EndDragDropTarget();
  }
}

void SceneHierarchyPanel::draw_context_menu() {
  const bool has_context = _selected_entity != flecs::entity::null();

  auto* vfs = App::get_system<VFS>(EngineSystems::VFS);
  const auto objects_dir = vfs->resolve_physical_dir(VFS::APP_DIR, "Objects");

  flecs::entity to_select = flecs::entity::null();

  ImGuiScoped::StyleVar styleVar1(ImGuiStyleVar_ItemInnerSpacing, {0, 5});
  ImGuiScoped::StyleVar styleVar2(ImGuiStyleVar_ItemSpacing, {1, 5});
  if (ImGui::BeginMenu("Create")) {
    if (ImGui::MenuItem("Empty Entity")) {
      to_select = _scene->create_entity();
    }

    if (ImGui::BeginMenu("Primitives")) {
      if (ImGui::MenuItem("Cube")) {
        to_select = _scene->create_entity();
        // @OLD _scene->load_mesh(AssetManager::get_mesh_asset("Resources/Objects/cube.glb"));
      }
      if (ImGui::MenuItem("Plane")) {
        to_select = _scene->create_entity();
        // @OLD _scene->load_mesh(AssetManager::get_mesh_asset("Resources/Objects/plane.glb"));
      }
      if (ImGui::MenuItem("Sphere")) {
        to_select = _scene->create_entity();
        // @OLD _scene->load_mesh(AssetManager::get_mesh_asset("Resources/Objects/sphere.glb"));
      }

      ImGui::EndMenu();
    }

    auto* asset_man = App::get_asset_manager();

    if (ImGui::MenuItem("Sprite")) {
      to_select = _scene->create_entity().add<SpriteComponent>();
      to_select.get_mut<SpriteComponent>()->material = asset_man->create_asset(AssetType::Material, {});
      asset_man->load_material(to_select.get_mut<SpriteComponent>()->material, Material{});
    }

    if (ImGui::MenuItem("Camera")) {
      to_select = _scene->create_entity();
      to_select.add<CameraComponent>().get_mut<TransformComponent>()->rotation.y = glm::radians(-90.f);
    }

    if (ImGui::MenuItem("Lua Script")) {
      to_select = _scene->create_entity().add<LuaScriptComponent>();
    }

    if (ImGui::BeginMenu("Light")) {
      if (ImGui::MenuItem("Light")) {
        to_select = _scene->create_entity().add<LightComponent>();
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Physics")) {
      if (ImGui::MenuItem("Sphere")) {
        to_select =
            _scene->create_entity().add<RigidbodyComponent>().add<SphereColliderComponent>().add<MeshComponent>();
        // @OLD _scene->registry.emplace<MeshComponent>(to_select,
        // AssetManager::get_mesh_asset("Resources/Objects/sphere.glb"));
      }

      if (ImGui::MenuItem("Cube")) {
        to_select =
            _scene->create_entity("Cube").add<RigidbodyComponent>().add<BoxColliderComponent>().add<MeshComponent>();
        // @OLD _scene->registry.emplace<MeshComponent>(to_select,
        // AssetManager::get_mesh_asset("Resources/Objects/cube.glb"));
      }

      if (ImGui::MenuItem("Character Controller")) {
        to_select =
            _scene->create_entity("Character Controller").add<CharacterControllerComponent>().add<MeshComponent>();
        // @OLD _scene->registry.emplace<MeshComponent>(to_select,
        // AssetManager::get_mesh_asset("Resources/Objects/capsule.glb"));
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Audio")) {
      if (ImGui::MenuItem("Audio Source")) {
        to_select = _scene->create_entity().add<AudioSourceComponent>();
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::MenuItem("Audio Listener")) {
        to_select = _scene->create_entity("AudioListener").add<AudioListenerComponent>();
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Effects")) {
      if (ImGui::MenuItem("PostProcess Probe")) {
        to_select = _scene->create_entity("PostProcess Probe").add<PostProcessProbe>();
      }
      if (ImGui::MenuItem("Particle System")) {
        to_select = _scene->create_entity("Particle System").add<ParticleSystemComponent>();
      }
      ImGui::EndMenu();
    }

    ImGui::EndMenu();
  }

  if (has_context && to_select != flecs::entity::null())
    to_select.child_of(_selected_entity);

  if (to_select != flecs::entity::null())
    _selected_entity = to_select;
}

auto SceneHierarchyPanel::on_update() -> void {
  if (_selected_entity != flecs::entity::null()) {
    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_D)) {
      _selected_entity.clone(true);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) &&
        (_table_hovered || EditorLayer::get()->viewport_panels[0]->is_viewport_hovered)) {
      _selected_entity.destruct();
      _selected_entity = flecs::entity::null();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
      _renaming_entity = _selected_entity;
    }
  }

  if (_deleted_entity != flecs::entity::null()) {
    auto& arch = EditorLayer::get()->advance_history();
    arch << static_cast<uint32_t>(HistoryOp::Delete);

    if (_selected_entity.id() == _deleted_entity.id())
      _selected_entity = flecs::entity::null();

    _deleted_entity.destruct();
    _deleted_entity = flecs::entity::null();
  }
}

auto SceneHierarchyPanel::on_render(vuk::Extent3D extent,
                                    vuk::Format format) -> void {
  ImGuiScoped::StyleVar cellpad(ImGuiStyleVar_CellPadding, {0, 0});

  if (on_begin(ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar)) {
    const float line_height = ImGui::GetTextLineHeight();

    const ImVec2 padding = ImGui::GetStyle().FramePadding;
    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_BordersInner | ImGuiTableFlags_ScrollY;

    const float filter_cursor_pos_x = ImGui::GetCursorPosX();
    _filter.Draw("###HierarchyFilter",
                 ImGui::GetContentRegionAvail().x - (ui::get_icon_button_size(ICON_MDI_PLUS, "").x + 2.0f * padding.x));
    ImGui::SameLine();

    if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_PLUS)))
      ImGui::OpenPopup("SceneHierarchyContextWindow");

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 8.0f));
    if (ImGui::BeginPopupContextWindow("SceneHierarchyContextWindow",
                                       ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
      draw_context_menu();
      ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

    if (!_filter.IsActive()) {
      ImGui::SameLine();
      ImGui::SetCursorPosX(filter_cursor_pos_x + ImGui::GetFontSize() * 0.5f);
      ImGui::TextUnformatted(StringUtils::from_char8_t(ICON_MDI_MAGNIFY " Search..."));
    }

    const ImVec2 cursor_pos = ImGui::GetCursorPos();
    const ImVec2 region = ImGui::GetContentRegionAvail();
    if (region.x != 0.0f && region.y != 0.0f)
      ImGui::InvisibleButton("##DragDropTargetBehindTable", region);
    drag_drop_target();

    ImGui::SetCursorPos(cursor_pos);
    if (ImGui::BeginTable("HierarchyTable", 3, table_flags)) {
      ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoClip);
      ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, line_height * 3.0f);
      ImGui::TableSetupColumn(StringUtils::from_char8_t("  " ICON_MDI_EYE_OUTLINE),
                              ImGuiTableColumnFlags_WidthFixed,
                              line_height * 2.0f);

      ImGui::TableSetupScrollFreeze(0, 1);

      ImGui::TableNextRow(ImGuiTableRowFlags_Headers, ImGui::GetFrameHeight());

      for (int column = 0; column < 3; ++column) {
        ImGui::TableSetColumnIndex(column);
        const char* column_name = ImGui::TableGetColumnName(column);
        ImGui::PushID(column);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + padding.y);
        ImGui::TableHeader(column_name);
        ImGui::PopID();
      }

      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
      _scene->world.query<TransformComponent>().each([this](const flecs::entity e, TransformComponent) {
        if (e.parent() == flecs::entity::null())
          draw_entity_node(e);
      });
      ImGui::PopStyleVar();

      const auto pop_item_spacing = ImGuiLayer::popup_item_spacing;
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, pop_item_spacing);
      if (ImGui::BeginPopupContextWindow("SceneHierarchyContextWindow",
                                         ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        _selected_entity = flecs::entity::null();
        draw_context_menu();
        ImGui::EndPopup();
      }
      ImGui::PopStyleVar();

      ImGui::EndTable();

      _table_hovered = ImGui::IsItemHovered();

      if (ImGui::IsItemClicked())
        _selected_entity = flecs::entity::null();
    }
    _window_hovered = ImGui::IsWindowHovered();

    if (ImGui::IsMouseDown(0) && _window_hovered)
      _selected_entity = flecs::entity::null();

    if (_dragged_entity != flecs::entity::null() && _dragged_entity_target != flecs::entity::null()) {
      _dragged_entity.child_of(_dragged_entity_target);
      _dragged_entity = flecs::entity::null();
      _dragged_entity_target = flecs::entity::null();
    }

    on_end();
  }
}
} // namespace ox
