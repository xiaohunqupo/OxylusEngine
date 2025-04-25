#pragma once

namespace sol {
class state;
}

namespace ox::LuaBindings {
void bind_renderer(const Shared<sol::state>& state);
}
