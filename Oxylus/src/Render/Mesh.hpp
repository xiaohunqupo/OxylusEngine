#pragma once

#include <glm/detail/type_quat.hpp>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "Assets/Material.hpp"

#include <vuk/Buffer.hpp>

#include "BoundingVolume.hpp"
#include "MeshVertex.hpp"

#include "Core/Types.hpp"

namespace fastgltf {
class Asset;
}
namespace ox {
class Mesh : public Asset {
public:
  struct Meshlet {
    uint32_t vertex_offset = 0;
    uint32_t index_offset = 0;
    uint32_t primitive_offset = 0;
    uint32_t index_count = 0;
    uint32_t primitive_count = 0;
    uint32_t material_id = 0;
    uint32_t instance_id = 0;
    AABB aabb = {};
  };

  struct Node {
    std::string name;

    Vec3 translation = {};
    Quat rotation = {};
    Vec3 scale = Vec3(1.0f);
    Mat4 transform = {};

    AABB aabb;

    uint32_t index = 0;
    Node* parent = nullptr;
    std::vector<Node*> children = {};
    std::vector<Meshlet> meshlets;
    std::vector<Shared<Material>> materials;

    Mat4 get_local_transform() const { return translate(Mat4(1.0f), translation) * Mat4(rotation) * glm::scale(Mat4(1.0f), scale) * transform; }

    Mat4 get_world_transform() const {
      Mat4 m = transform;
      auto p = parent;
      while (p) {
        m = p->transform * m;
        p = p->parent;
      }
      return m;
    }
  };

  struct SceneFlattened {
    std::vector<Meshlet> meshlets;
    std::vector<const Node*> nodes;
  };

  Mesh::SceneFlattened flattened;

  std::string path;
  std::vector<Node*> rootNodes;
  std::list<Node> nodes;
  std::vector<Vertex> _vertices;
  std::vector<uint> _indices;
  std::vector<uint8_t> primitives;
  std::vector<Shared<Material>> materials;

  mutable size_t previousMeshletsSize{};
  mutable size_t previousTransformsSize{};
  mutable size_t previousLightsSize{};

  uint index_count = 0;
  uint vertex_count = 0;
  vuk::Unique<vuk::Buffer> vertex_buffer;
  vuk::Unique<vuk::Buffer> index_buffer;

  Mesh() = default;
  ~Mesh() = default;
  Mesh(std::string_view path);
  Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

  void load_from_file(const std::string& file_path, glm::mat4 rootTransform = glm::identity<glm::mat4>());

  [[nodiscard]] SceneFlattened flatten() const;

  const Mesh* bind_vertex_buffer(vuk::CommandBuffer& command_buffer) const;
  const Mesh* bind_index_buffer(vuk::CommandBuffer& command_buffer) const;

private:
  [[nodiscard]] std::vector<Shared<Texture>> load_images(const fastgltf::Asset& asset);
  [[nodiscard]] std::vector<Shared<Material>> load_materials(const fastgltf::Asset& asset, const std::vector<Shared<Texture>>& images);
};
} // namespace ox
