#pragma once

#include <flecs.h>
#include <sol/usertype.hpp>

#include "Scripting/LuaBinding.hpp"

namespace ox {
class FlecsBinding : public LuaBinding {
public:
  sol::usertype<flecs::entity> entity_type;

  auto bind(sol::state* state) -> void override;
};
} // namespace ox
