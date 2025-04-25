#pragma once

namespace sol {
class state;
}

namespace ox::LuaBindings {
void bind_asset_manager(const Shared<sol::state>& state);
}
