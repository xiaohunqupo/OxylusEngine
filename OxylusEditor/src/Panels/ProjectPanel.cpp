#include "ProjectPanel.hpp"

#include <icons/IconsMaterialDesignIcons.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Core/FileSystem.hpp"
#include "Core/Project.hpp"
#include "EditorLayer.hpp"

#include "UI/OxUI.hpp"
#include "Utils/EditorConfig.hpp"
#include "Utils/StringUtils.hpp"

namespace ox {
ProjectPanel::ProjectPanel() : EditorPanel("Projects", ICON_MDI_ACCOUNT_BADGE, true) {}

void ProjectPanel::on_update() {}

void ProjectPanel::load_project_for_editor(const std::string& filepath) {
  if (Project::load(filepath)) {
    const auto startScene = App::get_absolute(Project::get_active()->get_config().start_scene);
    EditorLayer::get()->open_scene(startScene);
    EditorConfig::get()->add_recent_project(Project::get_active().get());
    EditorLayer::get()->get_panel<ContentPanel>()->invalidate();
    visible = false;
  }
}

void ProjectPanel::new_project(const std::string& project_dir, const std::string& project_name, const std::string& project_asset_dir) {
  const auto project = Project::new_project(project_dir, project_name, project_asset_dir);

  if (project)
    EditorConfig::get()->add_recent_project(project.get());
}

void ProjectPanel::on_render(vuk::Extent3D extent, vuk::Format format) {
  if (visible && !ImGui::IsPopupOpen("ProjectSelector"))
    ImGui::OpenPopup("ProjectSelector");
  constexpr auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration |
                         ImGuiWindowFlags_NoDocking;
  static bool draw_new_project_panel = false;

  ui::center_next_window();
  if (ImGui::BeginPopupModal("ProjectSelector", nullptr, flags)) {
    const float x = ImGui::GetContentRegionAvail().x;
    const float y = ImGui::GetFrameHeight();

    const auto banner_size = EditorLayer::get()->engine_banner->get_extent();

    const auto& window = App::get()->get_window();
    const float32 scale = window.get_content_scale();

    ui::image(*EditorLayer::get()->engine_banner, {static_cast<float>(banner_size.width) * scale, static_cast<float>(banner_size.height) * scale});
    ui::spacing(2);
    ImGui::SeparatorText("Projects");
    ui::spacing(2);

    if (draw_new_project_panel) {
      static std::string project_dir = {};
      static std::string project_name = "New Project";
      static std::string project_asset_dir = "Assets";
      ui::begin_properties();
      {
        ui::begin_property_grid("Directory", nullptr, false);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.8f);
        ImGui::InputText("##Directory", &project_dir, flags);
        ImGui::SameLine();
        if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_FOLDER), {ImGui::GetContentRegionAvail().x, 0})) {
          FileDialogFilter dialog_filters[] = {{.name = "Project dir", .pattern = ""}};
          window.show_dialog({
            .kind = DialogKind::OpenFolder,
            .user_data = &project_dir,
            .callback =
              [](void* user_data, const char8* const* files, int32) {
            auto& dst_path = *static_cast<std::string*>(user_data);
            if (!files || !*files) {
              return;
            }

            const auto first_path_cstr = *files;
            const auto first_path_len = std::strlen(first_path_cstr);
            dst_path = std::string(first_path_cstr, first_path_len);
          },
            .title = "Project dir...",
            .default_path = fs::current_path(),
            .filters = dialog_filters,
            .multi_select = false,
          });
          project_dir = fs::append_paths(project_dir, project_name);
        }
        ui::end_property_grid();
      }
      ui::property("Name", &project_name);
      ui::property("Asset Directory", &project_asset_dir);
      ui::end_properties();
      ImGui::Separator();
      ImGui::SetNextItemWidth(-1);
      if (ImGui::Button("Create", ImVec2(120, 0))) {
        new_project(project_dir, project_name, project_asset_dir);
        visible = false;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SetItemDefaultFocus();
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        draw_new_project_panel = false;
      }
    } else {
      const auto projects = EditorConfig::get()->get_recent_projects();
      for (auto& project : projects) {
        auto project_name = fs::get_file_name(project);
        if (ImGui::Button(project_name.c_str(), {x, y})) {
          load_project_for_editor(project);
        }
      }

      ImGui::Separator();
      if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_FILE_DOCUMENT " New Project"), {x, y})) {
        draw_new_project_panel = true;
      }
      ImGui::SetNextItemWidth(x);
      if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_UPLOAD " Load Project"), {x, y})) {
        std::string path = {};
        FileDialogFilter dialog_filters[] = {{.name = "Oxylus Project", .pattern = "oxproj"}};
        window.show_dialog({
          .kind = DialogKind::OpenFile,
          .user_data = &path,
          .callback =
            [](void* user_data, const char8* const* files, int32) {
          auto& dst_path = *static_cast<std::string*>(user_data);
          if (!files || !*files) {
            return;
          }

          const auto first_path_cstr = *files;
          const auto first_path_len = std::strlen(first_path_cstr);
          dst_path = std::string(first_path_cstr, first_path_len);
        },
          .title = "Open project...",
          .default_path = fs::current_path(),
          .filters = dialog_filters,
          .multi_select = false,
        });
        if (!path.empty()) {
          load_project_for_editor(path);
        }
      }
      ui::align_right(ImVec2(120, 0).x);
      if (ImGui::Button("Skip", ImVec2(120, 0))) {
        visible = false;
        ImGui::CloseCurrentPopup();
      }
    }
    ui::spacing(4);
    ImGui::EndPopup();
  }
}
} // namespace ox
