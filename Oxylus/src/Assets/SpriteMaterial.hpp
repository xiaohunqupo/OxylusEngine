#pragma once

#include "Material.hpp"

#include "Texture.hpp"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace ox {
class SpriteMaterial : public Material {
public:
  struct Parameters {
    glm::vec4 color = glm::vec4(1.f);

    glm::vec2 uv_size = glm::vec2(1.f);
    glm::vec2 uv_offset = glm::vec2(0.f);

    uint32_t albedo_map_id = Asset::INVALID_ID;
  } parameters;

  SpriteMaterial(std::string name = "new_material");

  Shared<Texture>& get_albedo_texture() { return albedo_texture; }

  SpriteMaterial* set_albedo_texture(const Shared<Texture>& texture);

private:
  Shared<Texture> albedo_texture = nullptr;
};
} // namespace ox
