#pragma once

namespace sol {
class state;
}

namespace ox::LuaBindings {
void bind_ui(const Shared<sol::state>& state);
}
