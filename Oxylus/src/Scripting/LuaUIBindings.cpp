#include "LuaUIBindings.hpp"

#include <sol/state.hpp>

#include "LuaImGuiBindings.hpp"

namespace ox::LuaBindings {
void bind_ui(sol::state* state) {
  LuaImGuiBindings::init(state);

  // TODO: the rest of the api
}
} // namespace ox::LuaBindings
