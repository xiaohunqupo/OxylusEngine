#include "Scripting/LuaApplicationBindings.hpp"

#include <sol/state.hpp>

#include "Asset/AssetManager.hpp"
#include "Core/App.hpp"
#include "Core/VFS.hpp"
#include "Scripting/LuaHelpers.hpp"

namespace ox {
auto AppBinding::bind(sol::state* state) -> void {
  auto app = state->create_table("App");

  SET_TYPE_FUNCTION(app, App, get_asset_manager);
  SET_TYPE_FUNCTION(app, App, get_vfs);
}
} // namespace ox
