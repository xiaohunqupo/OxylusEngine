#pragma once

#include <sol/state.hpp>

#include "Core/ESystem.hpp"

namespace ox {
class LuaManager : public ESystem {
public:
  auto init() -> std::expected<void, std::string> override;
  auto deinit() -> std::expected<void, std::string> override;

  auto get_state() const -> sol::state* { return _state.get(); }

private:
  std::unique_ptr<sol::state> _state = nullptr;

  void bind_log() const;
};
} // namespace ox
