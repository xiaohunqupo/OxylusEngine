#include "Scripting/LuaAssetManagerBindings.hpp"

#include <sol/state.hpp>

#include "Asset/AssetManager.hpp"
#include "Scripting/LuaHelpers.hpp"

namespace ox {
auto AssetManagerBinding::bind(sol::state* state) -> void {
  auto uuid = state->new_usertype<UUID>("UUID");

  SET_TYPE_FUNCTION(uuid, UUID, str);

  auto asset_manager = state->new_usertype<AssetManager>("AssetManager");

  SET_TYPE_FUNCTION(asset_manager, AssetManager, import_asset);
  SET_TYPE_FUNCTION(asset_manager, AssetManager, load_asset);
  SET_TYPE_FUNCTION(asset_manager, AssetManager, unload_asset);
}
} // namespace ox
