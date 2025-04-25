#pragma once

#include "Scene/Components.hpp"

namespace ox {
struct SkyboxLoadEvent {
  Shared<Texture> cube_map = nullptr;
};
}
