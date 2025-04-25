#pragma once

#include "Core/ESystem.hpp"

namespace sol {
class state;
}

namespace ox {
class LuaManager : public ESystem {
public:
  void init() override;
  void deinit() override;
    
  sol::state* get_state() const { return m_state.get(); }

private:
  Shared<sol::state> m_state = nullptr;

  void bind_log() const;
};
}
