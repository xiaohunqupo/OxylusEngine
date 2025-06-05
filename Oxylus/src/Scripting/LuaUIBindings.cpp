#include "LuaUIBindings.hpp"

#include <sol/state.hpp>

#include "LuaImGuiBindings.hpp"

namespace ox::LuaBindings {
void bind_ui(const Shared<sol::state>& state) {
  LuaImGuiBindings::init(state.get());

  // TODO: the rest of the api
}
} // namespace ox::LuaBindings
