#pragma once

namespace sol {
class state;
}

namespace ox::LuaBindings {
void bind_components(sol::state* state);
}
