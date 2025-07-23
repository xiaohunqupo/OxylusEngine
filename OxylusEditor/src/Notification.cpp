#include "Notification.hpp"

#include <Tracy.hpp>
#include <fmt/format.h>
#include <icons/IconsMaterialDesignIcons.h>
#include <imgui_internal.h>
#include <imspinner.h>

namespace ox {
static constexpr auto notification_window_size = ImVec2(400.f, 50.f);
static constexpr f32 padding = 40.f;

auto NotificationSystem::add(Notification&& notif) -> void {
  ZoneScoped;

  if (active_notifications.contains(notif.title)) {
    if (notif.completed) {
      active_notifications.insert_or_assign(notif.title, std::move(notif));
      return;
    }
  }

  active_notifications.emplace(notif.title, std::move(notif));
}

auto NotificationSystem::draw() -> void {
  ZoneScoped;
  const auto now = std::chrono::steady_clock::now();
  // Bottom right
  ImVec2 screen_pos = ImGui::GetMainViewport()->Size;
  screen_pos.x -= notification_window_size.x + padding;
  screen_pos.y -= notification_window_size.y + padding;

  float y_offset = 0.0f;

  for (auto& [name, notif] : active_notifications) {
    draw_single(notif, now, screen_pos, y_offset);
    y_offset -= notification_window_size.y + 10.f;
  }

  // Cleanup
  const auto delay = std::chrono::seconds(2); // so that really fast notifications don't flash
  std::erase_if(active_notifications,
                [&](const auto& n) { return n.second.completed && (now - n.second.created_at) > delay; });
}

auto NotificationSystem::draw_single(Notification& notif, auto current_time, const ImVec2& screen_pos, f32 y_offset) -> void {
  ZoneScoped;

  ImGui::SetNextWindowBgAlpha(0.8f);
  ImGui::SetNextWindowPos({screen_pos.x, screen_pos.y + y_offset}, ImGuiCond_Always);
  ImGui::SetNextWindowSize(notification_window_size, ImGuiCond_Always);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.0f);
  if (ImGui::Begin(notif.title.c_str(),
                   nullptr,
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing)) {
    ImSpinner::detail::SpinnerConfig config{};
    config.setSpinnerType(ImSpinner::e_st_ang);
    config.setSpeed(6.f);
    config.setAngle(4.f);
    config.setThickness(2.f);
    config.setRadius(16.f);
    config.setColor(ImColor(1.f, 1.f, 1.f, 1.f));
    ImSpinner::Spinner("SpinnerAng270NoBg", config);
    ImGui::SameLine();
    ImGui::BeginChild("##load_text");
    ImGui::Text("Loading...");
    ImGui::TextUnformatted(fmt::format("{}", notif.title).c_str());
    ImGui::EndChild();

    ImGui::End();
  }
  ImGui::PopStyleVar();
}
} // namespace ox
