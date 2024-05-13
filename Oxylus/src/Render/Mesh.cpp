#include "Mesh.hpp"

#include <execution>

#include "vuk/Types.hpp"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <ktx.h>
#include <stb_image.h>

#include <meshoptimizer.h>
#include <ranges>
#include <stack>

#include <glm/gtc/type_ptr.hpp>
#include <vuk/vsl/Core.hpp>

#include "Assets/AssetManager.hpp"
#include "Core/FileSystem.hpp"

#include "Scene/Components.hpp"
#include "Scene/Entity.hpp"

#include "Scene/Scene.hpp"

#include "Utils/Log.hpp"
#include "Utils/Profiler.hpp"
#include "Utils/Timer.hpp"

#include "Vulkan/VkContext.hpp"
#include "fastgltf/core.hpp"

namespace ox {
Mesh::Mesh(const std::string_view path) { load_from_file(path.data()); }

Mesh::SceneFlattened Mesh::flatten() const {
  SceneFlattened sceneFlattened;

  struct StackElement {
    const Node* node;
    Mat4 parentGlobalTransform;
  };
  std::stack<StackElement> nodeStack;

  for (auto* rootNode : rootNodes) {
    nodeStack.emplace(rootNode, rootNode->get_local_transform());
  }

  // Traverse the scene
  while (!nodeStack.empty()) {
    auto [node, parentGlobalTransform] = nodeStack.top();
    nodeStack.pop();

    sceneFlattened.nodes.emplace_back(node);

    const auto globalTransform = parentGlobalTransform * node->get_local_transform();

    for (const auto* childNode : node->children) {
      nodeStack.emplace(childNode, globalTransform);
    }

    if (!node->meshlets.empty()) {
      const auto instanceId = sceneFlattened.transforms.size();
      sceneFlattened.transforms.emplace_back(globalTransform);
      for (auto meshlet : node->meshlets) {
        meshlet.instance_id = static_cast<uint32_t>(instanceId);
        sceneFlattened.meshlets.emplace_back(meshlet);
      }
    }
  }

  sceneFlattened.materials = _materials;

  return sceneFlattened;
}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32>& indices) {
  this->_vertices = vertices;
  this->_indices = indices;

  const auto context = VkContext::get();
  auto compiler = vuk::Compiler{};

  auto [vBuffer, vBufferFut] = create_buffer(*context->superframe_allocator,
                                             vuk::MemoryUsage::eGPUonly,
                                             vuk::DomainFlagBits::eTransferOnGraphics,
                                             std::span(_vertices));

  vBufferFut.wait(*context->superframe_allocator, compiler);
  vertex_buffer = std::move(vBuffer);

  auto [iBuffer, iBufferFut] = create_buffer(*context->superframe_allocator,
                                             vuk::MemoryUsage::eGPUonly,
                                             vuk::DomainFlagBits::eTransferOnGraphics,
                                             std::span(_indices));

  iBufferFut.wait(*context->superframe_allocator, compiler);
  index_buffer = std::move(iBuffer);

  index_count = (uint32)indices.size();
}

static std::vector<Vertex> ConvertVertexBufferFormat(const fastgltf::Asset& model,
                                                     std::size_t positionAccessorIndex,
                                                     std::size_t normalAccessorIndex,
                                                     std::optional<std::size_t> texcoordAccessorIndex,
                                                     std::optional<std::size_t> color_accessor_index) {
  OX_SCOPED_ZONE;
  std::vector<float3> positions;
  auto& positionAccessor = model.accessors[positionAccessorIndex];
  positions.resize(positionAccessor.count);
  fastgltf::iterateAccessorWithIndex<float3>(model, positionAccessor, [&](float3 position, std::size_t idx) { positions[idx] = position; });

  std::vector<float3> normals;
  auto& normalAccessor = model.accessors[normalAccessorIndex];
  normals.resize(normalAccessor.count);
  fastgltf::iterateAccessorWithIndex<float3>(model, normalAccessor, [&](float3 normal, std::size_t idx) { normals[idx] = normal; });

  std::vector<float2> texcoords;

  // Textureless meshes will use factors instead of textures
  if (texcoordAccessorIndex.has_value()) {
    auto& texcoordAccessor = model.accessors[texcoordAccessorIndex.value()];
    texcoords.resize(texcoordAccessor.count);
    fastgltf::iterateAccessorWithIndex<float2>(model, texcoordAccessor, [&](float2 texcoord, std::size_t idx) { texcoords[idx] = texcoord; });
  } else {
    // If no texcoord attribute, fill with empty texcoords to keep everything consistent and happy
    texcoords.resize(positions.size(), {});
  }

  std::vector<float4> colors; // TODO: Unused for now...
  if (color_accessor_index.has_value()) {
    auto& color_accessor = model.accessors[color_accessor_index.value()];
    colors.resize(color_accessor.count);
    fastgltf::iterateAccessorWithIndex<float3>(model, color_accessor, [&](float3 color, std::size_t idx) { colors[idx] = float4(color, 1.0f); });
  } else {
    colors.resize(positions.size(), float4(1.0f));
  }

  OX_ASSERT(positions.size() == normals.size() && positions.size() == texcoords.size() && positions.size() == colors.size());

  std::vector<Vertex> vertices;
  vertices.resize(positions.size());

  for (size_t i = 0; i < positions.size(); i++) {
    vertices[i] = {positions[i], glm::packSnorm2x16(math::float32x3_to_oct(normals[i])), texcoords[i]};
  }

  return vertices;
}

static std::vector<uint32> ConvertIndexBufferFormat(const fastgltf::Asset& model, std::size_t indicesAccessorIndex) {
  OX_SCOPED_ZONE;
  auto indices = std::vector<uint32>();
  auto& accessor = model.accessors[indicesAccessorIndex];
  indices.resize(accessor.count);
  fastgltf::iterateAccessorWithIndex<uint32>(model, accessor, [&](uint32 index, size_t idx) { indices[idx] = index; });
  return indices;
}

glm::mat4 NodeToMat4(const fastgltf::Node& node) {
  glm::mat4 transform{1};

  if (auto* trs = std::get_if<fastgltf::TRS>(&node.transform)) {
    // Note: do not use glm::make_quat because glm and glTF use different quaternion component layouts (wxyz vs xyzw)!
    const auto rotation = glm::quat{trs->rotation[3], trs->rotation[0], trs->rotation[1], trs->rotation[2]};
    const auto scale = glm::make_vec3(trs->scale.data());
    const auto translation = glm::make_vec3(trs->translation.data());

    const glm::mat4 rotationMat = glm::mat4_cast(rotation);

    // T * R * S
    transform = glm::translate(Mat4(1.0f), translation) * rotationMat, glm::scale(Mat4(1.0f), scale);
  } else if (auto* mat = std::get_if<fastgltf::Node::TransformMatrix>(&node.transform)) {
    transform = glm::make_mat4(mat->data());
  }
  // else node has identity transform

  return transform;
}

constexpr auto maxMeshletIndices = 64u;
constexpr auto maxMeshletPrimitives = 64u;
constexpr auto meshletConeWeight = 0.0f;

struct RawMesh {
  std::vector<Vertex> vertices;
  std::vector<uint32> indices;
  AABB boundingBox;
};

struct NodeTempData {
  struct Indices {
    size_t rawMeshIndex;
    size_t materialIndex;
  };
  std::vector<Indices> indices;
};

struct LoadModelResult {
  std::list<Mesh::Node> nodes;
  // This is a semi-hacky way to extend `nodes` without changing the node type, so we can still splice our nodes onto the main scene's
  std::vector<NodeTempData> tempData;
  std::vector<RawMesh> rawMeshes;
  std::vector<Shared<Material>> materials;
};

void Mesh::load_from_file(const std::string& file_path, glm::mat4 rootTransform) {
  OX_SCOPED_ZONE;

  Timer timer;

  path = file_path;

  const auto extension = fs::get_file_extension(file_path);
  const auto isText = extension == "gltf";
  const auto isBinary = extension == "glb";

  if (!isText && !isBinary) {
    OX_LOG_ERROR("Only glTF files(.gltf/.glb) are supported!");
    return;
  }

  auto maybeAsset = [&]() -> fastgltf::Expected<fastgltf::Asset> {
    OX_SCOPED_ZONE_N("Parse glTF");
    using fastgltf::Extensions;
    constexpr auto gltfExtensions = Extensions::KHR_texture_basisu | Extensions::KHR_mesh_quantization | Extensions::EXT_meshopt_compression |
                                    Extensions::KHR_lights_punctual | Extensions::KHR_materials_emissive_strength;
    auto parser = fastgltf::Parser(gltfExtensions);

    auto data = fastgltf::GltfDataBuffer();
    data.loadFromFile(file_path);

    constexpr auto options = fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages | fastgltf::Options::LoadGLBBuffers;
    if (isBinary) {
      return parser.loadGltfBinary(&data, fs::get_directory(file_path), options);
    }

    return parser.loadGltfJson(&data, fs::get_directory(file_path), options);
  }();

  if (auto err = maybeAsset.error(); err != fastgltf::Error::None) {
    OX_LOG_ERROR("glTF error: {}", fastgltf::getErrorMessage(err));
    return;
  }

  const auto& asset = maybeAsset.get();

  OX_ASSERT(asset.scenes.size() == 1, "Multiple scenes are not supported for now...");

  auto images = load_images(asset);
  _materials = load_materials(asset, images);

  LoadModelResult scene;

  struct AccessorIndices {
    std::optional<std::size_t> positionsIndex;
    std::optional<std::size_t> normalsIndex;
    std::optional<std::size_t> texcoordsIndex;
    std::optional<std::size_t> colorsIndex;
    std::optional<std::size_t> indicesIndex;

    auto operator<=>(const AccessorIndices&) const = default;
  };

  auto uniqueAccessorCombinations = std::vector<std::pair<AccessorIndices, std::size_t>>();

  // <node*, global transform>
  struct StackElement {
    Node* sceneNode{};
    const fastgltf::Node* gltfNode{};
    size_t tempDataIndex;
  };
  std::stack<StackElement> nodeStack;

  uint32 node_index = 0;
  for (const fastgltf::Node& node : asset.nodes) {
    const auto name = node.name.empty() ? std::string("Node") : std::string(node.name);
    Node* sceneNode = &scene.nodes.emplace_back(name);
    sceneNode->index = node_index;
    nodeStack.emplace(sceneNode, &node, scene.tempData.size());
    scene.tempData.emplace_back();

    std::visit(fastgltf::visitor{[&](const fastgltf::Node::TransformMatrix& matrix) { memcpy(&sceneNode->transform, matrix.data(), sizeof(matrix)); },
                                 [&](const fastgltf::TRS& transform) {
      const glm::vec3 tl(transform.translation[0], transform.translation[1], transform.translation[2]);
      const glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
      const glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

      sceneNode->translation = tl;
      sceneNode->rotation = rot;
      sceneNode->scale = sc;

      const glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
      const glm::mat4 rm = glm::toMat4(rot);
      const glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

      sceneNode->transform = tm * rm * sm;
    }},
               node.transform);
    node_index++;
  }

  while (!nodeStack.empty()) {
    decltype(nodeStack)::value_type top = nodeStack.top();
    const auto& [node, gltfNode, tempDataIndex] = top;
    nodeStack.pop();

    const glm::mat4 localTransform = NodeToMat4(*gltfNode);

    std::array<float, 16> localTransformArray{};
    std::copy_n(&localTransform[0][0], 16, localTransformArray.data());
    std::array<float, 3> scaleArray{};
    std::array<float, 4> rotationArray{};
    std::array<float, 3> translationArray{};
    fastgltf::decomposeTransformMatrix(localTransformArray, scaleArray, rotationArray, translationArray);

    node->translation = glm::make_vec3(translationArray.data());
    node->rotation = {rotationArray[3], rotationArray[0], rotationArray[1], rotationArray[2]};
    node->scale = glm::make_vec3(scaleArray.data());

    for (auto childNodeIndex : gltfNode->children) {
      const auto& assetNode = asset.nodes[childNodeIndex];
      const auto name = assetNode.name.empty() ? std::string("Node") : std::string(assetNode.name);
      Node* childSceneNode = &scene.nodes.emplace_back(name);
      node->children.emplace_back(childSceneNode);
      nodeStack.emplace(childSceneNode, &assetNode, scene.tempData.size());
      scene.tempData.emplace_back();
    }

    if (gltfNode->meshIndex.has_value()) {
      // Load each primitive in the mesh
      for (const fastgltf::Mesh& mesh = asset.meshes[gltfNode->meshIndex.value()]; const auto& primitive : mesh.primitives) {
        AccessorIndices accessorIndices;
        if (auto it = primitive.findAttribute("POSITION"); it != primitive.attributes.end()) {
          accessorIndices.positionsIndex = it->second;
        } else {
          OX_ASSERT(false);
        }

        if (auto it = primitive.findAttribute("NORMAL"); it != primitive.attributes.end()) {
          accessorIndices.normalsIndex = it->second;
        } else {
          // TODO: calculate normal
          OX_ASSERT(false);
        }

        if (auto it = primitive.findAttribute("TEXCOORD_0"); it != primitive.attributes.end()) {
          accessorIndices.texcoordsIndex = it->second;
        } else {
          // Okay, texcoord can be safely missing
        }

        if (auto it = primitive.findAttribute("COLOR_0"); it != primitive.attributes.end()) {
          accessorIndices.colorsIndex = it->second;
        }

        OX_ASSERT(primitive.indicesAccessor.has_value() && "Non-indexed meshes are not supported");
        accessorIndices.indicesIndex = *primitive.indicesAccessor;

        size_t rawMeshIndex = uniqueAccessorCombinations.size();

        // Only emplace and increment counter if combo does not exist
        if (auto it = std::ranges::find_if(uniqueAccessorCombinations, [&](const auto& p) { return p.first == accessorIndices; });
            it == uniqueAccessorCombinations.end()) {
          uniqueAccessorCombinations.emplace_back(accessorIndices, rawMeshIndex);
        } else {
          // Combo already exists
          rawMeshIndex = it->second;
        }

        const auto materialId = primitive.materialIndex.value_or(0);

        scene.tempData[tempDataIndex].indices.emplace_back(rawMeshIndex, materialId);
      }
    }
  }

  scene.rawMeshes.resize(uniqueAccessorCombinations.size());

  std::transform(std::execution::par,
                 uniqueAccessorCombinations.begin(),
                 uniqueAccessorCombinations.end(),
                 scene.rawMeshes.begin(),
                 [&](const std::pair<AccessorIndices, std::size_t>& keyValue) -> RawMesh {
    OX_SCOPED_ZONE_N("Convert vertices and indices");
    const auto& [accessorIndices, _] = keyValue;
    auto vertices = ConvertVertexBufferFormat(asset,
                                              accessorIndices.positionsIndex.value(),
                                              accessorIndices.normalsIndex.value(),
                                              accessorIndices.texcoordsIndex,
                                              accessorIndices.colorsIndex);
    auto indices = ConvertIndexBufferFormat(asset, accessorIndices.indicesIndex.value());

    const auto& positionAccessor = asset.accessors[accessorIndices.positionsIndex.value()];

    glm::vec3 bboxMin{};
    if (auto* dv = std::get_if<std::pmr::vector<double>>(&positionAccessor.min)) {
      bboxMin = {(*dv)[0], (*dv)[1], (*dv)[2]};
    }
    if (auto* iv = std::get_if<std::pmr::vector<int64_t>>(&positionAccessor.min)) {
      bboxMin = {(*iv)[0], (*iv)[1], (*iv)[2]};
    }

    glm::vec3 bboxMax{};
    if (auto* dv = std::get_if<std::pmr::vector<double>>(&positionAccessor.max)) {
      bboxMax = {(*dv)[0], (*dv)[1], (*dv)[2]};
    }
    if (auto* iv = std::get_if<std::pmr::vector<int64_t>>(&positionAccessor.max)) {
      bboxMax = {(*iv)[0], (*iv)[1], (*iv)[2]};
    }

    return RawMesh{
      .vertices = std::move(vertices),
      .indices = std::move(indices),
      .boundingBox = AABB{bboxMin, bboxMax},
    };
  });

  const auto baseVertexOffset = _vertices.size();
  const auto baseIndexOffset = _indices.size();
  const auto basePrimitiveOffset = primitives.size();

  uint32_t vertexOffset = (uint32_t)baseVertexOffset;
  uint32_t indexOffset = (uint32_t)baseIndexOffset;
  uint32_t primitiveOffset = (uint32_t)basePrimitiveOffset;

  struct MeshletInfo {
    const RawMesh* rawMeshPtr{};
    std::span<const Vertex> vertices;
    std::vector<uint32_t> meshletIndices;
    std::vector<uint8_t> meshletPrimitives;
    std::vector<meshopt_Meshlet> rawMeshlets;
  };

  std::vector<MeshletInfo> meshletInfos(scene.rawMeshes.size());

  std::transform(std::execution::par, scene.rawMeshes.begin(), scene.rawMeshes.end(), meshletInfos.begin(), [](const RawMesh& mesh) -> MeshletInfo {
    OX_SCOPED_ZONE_N("Create meshlets for mesh");

    const auto maxMeshlets = meshopt_buildMeshletsBound(mesh.indices.size(), maxMeshletIndices, maxMeshletPrimitives);

    MeshletInfo meshletInfo;
    auto& [rawMeshPtr, vertices, meshletIndices, meshletPrimitives, rawMeshlets] = meshletInfo;

    rawMeshPtr = &mesh;
    vertices = mesh.vertices;
    meshletIndices.resize(maxMeshlets * maxMeshletIndices);
    meshletPrimitives.resize(maxMeshlets * maxMeshletPrimitives * 3);
    rawMeshlets.resize(maxMeshlets);

    const auto meshletCount = [&] {
      OX_SCOPED_ZONE_N("meshopt_buildMeshlets");
      return meshopt_buildMeshlets(rawMeshlets.data(),
                                   meshletIndices.data(),
                                   meshletPrimitives.data(),
                                   mesh.indices.data(),
                                   mesh.indices.size(),
                                   reinterpret_cast<const float*>(mesh.vertices.data()),
                                   mesh.vertices.size(),
                                   sizeof(Vertex),
                                   maxMeshletIndices,
                                   maxMeshletPrimitives,
                                   meshletConeWeight);
    }();

    const auto& lastMeshlet = rawMeshlets[meshletCount - 1];
    meshletIndices.resize(lastMeshlet.vertex_offset + lastMeshlet.vertex_count);
    meshletPrimitives.resize(lastMeshlet.triangle_offset + ((lastMeshlet.triangle_count * 3 + 3) & ~3));
    rawMeshlets.resize(meshletCount);

    return meshletInfo;
  });

  auto perMeshMeshlets = std::vector<std::vector<Mesh::Meshlet>>(meshletInfos.size());

  // For each mesh, create "meshlet templates" (meshlets without per-instance data) and copy vertices, indices, and primitives to mega buffers
  for (size_t meshIndex = 0; meshIndex < meshletInfos.size(); meshIndex++) {
    auto& [rawMesh, vert, meshletIndices, meshletPrimitives, rawMeshlets] = meshletInfos[meshIndex];
    perMeshMeshlets[meshIndex].reserve(rawMeshlets.size());

    for (const auto& meshlet : rawMeshlets) {
      auto min = glm::vec3(std::numeric_limits<float>::max());
      auto max = glm::vec3(std::numeric_limits<float>::lowest());
      for (uint32_t i = 0; i < meshlet.triangle_count * 3; ++i) {
        const auto& vertex = rawMesh->vertices[meshletIndices[meshlet.vertex_offset + meshletPrimitives[meshlet.triangle_offset + i]]];
        min = glm::min(min, vertex.position);
        max = glm::max(max, vertex.position);
      }

      perMeshMeshlets[meshIndex].emplace_back(Mesh::Meshlet{
        .vertex_offset = vertexOffset,
        .index_offset = indexOffset + meshlet.vertex_offset,
        .primitive_offset = primitiveOffset + meshlet.triangle_offset,
        .index_count = meshlet.vertex_count,
        .primitive_count = meshlet.triangle_count,
        .aabbMin = {min.x, min.y, min.z},
        .aabbMax = {max.x, max.y, max.z},
      });
    }

    vertexOffset += (uint32_t)vert.size();
    indexOffset += (uint32_t)meshletIndices.size();
    primitiveOffset += (uint32_t)meshletPrimitives.size();

    std::ranges::copy(vert, std::back_inserter(_vertices));
    std::ranges::copy(meshletIndices, std::back_inserter(_indices));
    std::ranges::copy(meshletPrimitives, std::back_inserter(primitives));
  }

  for (size_t i = 0; auto& node : scene.nodes) {
    if (!node.parent)
      rootNodes.emplace_back(&node);

    for (const auto& [rawMeshIndex, materialId] : scene.tempData[i++].indices) {
      for (auto& meshlet : perMeshMeshlets[rawMeshIndex]) {
        meshlet.material_id = (uint32)materialId;
        node.meshlets.emplace_back(meshlet);
        node.materials.emplace_back(_materials[meshlet.material_id]);
      }
    }
  }

  nodes.splice(nodes.end(), scene.nodes);

  flattened = flatten();

  index_count = (uint32)_indices.size();
  vertex_count = (uint32)_vertices.size();

  // create buffers
  auto ctx = VkContext::get();
  auto compiler = vuk::Compiler{};

  auto [vBuffer, vBufferFut] = create_buffer(*ctx->superframe_allocator,
                                             vuk::MemoryUsage::eGPUonly,
                                             vuk::DomainFlagBits::eTransferOnGraphics,
                                             std::span(_vertices));

  vBufferFut.wait(*ctx->superframe_allocator, compiler);
  vertex_buffer = std::move(vBuffer);

  auto [iBuffer, iBufferFut] = create_buffer(*ctx->superframe_allocator,
                                             vuk::MemoryUsage::eGPUonly,
                                             vuk::DomainFlagBits::eTransferOnGraphics,
                                             std::span(_indices));

  iBufferFut.wait(*ctx->superframe_allocator, compiler);
  index_buffer = std::move(iBuffer);

  OX_LOG_INFO("Loaded mesh {0}:{1}", fs::get_name_with_extension(path), timer.get_elapsed_ms());
}

const Mesh* Mesh::bind_vertex_buffer(vuk::CommandBuffer& command_buffer) const {
  OX_SCOPED_ZONE;
  command_buffer.bind_vertex_buffer(0, *vertex_buffer, 0, vertex_pack);
  return this;
}

const Mesh* Mesh::bind_index_buffer(vuk::CommandBuffer& command_buffer) const {
  OX_SCOPED_ZONE;
  command_buffer.bind_index_buffer(*index_buffer, vuk::IndexType::eUint32);
  return this;
}

std::vector<Shared<Texture>> Mesh::load_images(const fastgltf::Asset& asset) {
  struct RawImageData {
    // Used for ktx and non-ktx images alike
    std::unique_ptr<std::byte[]> encoded_pixel_data = {};
    std::size_t encoded_pixel_size = 0;

    bool is_ktx = false;
    vuk::Format format_ktx;
    int width1 = 0;
    int height = 0;
    std::string name;

    // Non-ktx. Raw decoded pixel data
    std::unique_ptr<unsigned char[]> data = {};

    // ktx
    std::unique_ptr<ktxTexture2, decltype([](ktxTexture2* p) { ktxTexture_Destroy(ktxTexture(p)); })> ktx = {};
  };

  auto make_raw_image_data = [](const void* data, std::size_t data_size, fastgltf::MimeType mime_type, std::string_view name) -> RawImageData {
    auto data_copy = std::make_unique<std::byte[]>(data_size);
    std::copy_n(static_cast<const std::byte*>(data), data_size, data_copy.get());

    return RawImageData{
      .encoded_pixel_data = std::move(data_copy),
      .encoded_pixel_size = data_size,
      .is_ktx = mime_type == fastgltf::MimeType::KTX2,
      .name = std::string(name),
    };
  };

  const auto indices = std::ranges::iota_view((size_t)0, asset.images.size());

  // Load and decode image data locally, in parallel
  auto raw_image_datas = std::vector<RawImageData>(asset.images.size());

  std::transform(std::execution::par, indices.begin(), indices.end(), raw_image_datas.begin(), [&](size_t index) {
    OX_SCOPED_ZONE_N("Load Image");
    const fastgltf::Image& image = asset.images[index];
    ZoneName(image.name.c_str(), image.name.size());

    RawImageData rawImage = [&] {
      if (const auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data)) {
        OX_ASSERT(filePath->fileByteOffset == 0, "File offsets are not supported.");
        OX_ASSERT(filePath->uri.isLocalPath(), "Only loading local files are supported.");

        const auto file_data = fs::read_file_binary(filePath->uri.path());

        return make_raw_image_data(file_data.data(), file_data.size(), filePath->mimeType, image.name);
      }

      if (const auto* vector = std::get_if<fastgltf::sources::Array>(&image.data)) {
        return make_raw_image_data(vector->bytes.data(), vector->bytes.size(), vector->mimeType, image.name);
      }

      if (const auto* view = std::get_if<fastgltf::sources::BufferView>(&image.data)) {
        auto& buffer_view = asset.bufferViews[view->bufferViewIndex];
        auto& buffer = asset.buffers[buffer_view.bufferIndex];
        if (const auto* vector = std::get_if<fastgltf::sources::Array>(&buffer.data)) {
          return make_raw_image_data(vector->bytes.data() + buffer_view.byteOffset, buffer_view.byteLength, view->mimeType, image.name);
        }
      }

      return RawImageData{};
    }();

    if (rawImage.is_ktx) {
      OX_SCOPED_ZONE_N("Decode KTX2");
      ktxTexture2* ktx{};
      if (const auto result = ktxTexture2_CreateFromMemory(reinterpret_cast<const ktx_uint8_t*>(rawImage.encoded_pixel_data.get()),
                                                           rawImage.encoded_pixel_size,
                                                           KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                                           &ktx);
          result != KTX_SUCCESS) {
        OX_LOG_ERROR("Couldn't load KTX2 file {}", ktxErrorString(result));
      }

      rawImage.format_ktx = vuk::Format::eBc7UnormBlock;
      constexpr ktx_transcode_fmt_e ktxTranscodeFormat = KTX_TTF_BC7_RGBA;

      // If the image needs is in a supercompressed encoding, transcode it to a desired format
      if (ktxTexture2_NeedsTranscoding(ktx)) {
        OX_SCOPED_ZONE_N("Transcode KTX 2 Texture");
        if (const auto result = ktxTexture2_TranscodeBasis(ktx, ktxTranscodeFormat, KTX_TF_HIGH_QUALITY); result != KTX_SUCCESS) {
          OX_LOG_ERROR("Couldn't transcode KTX2 file {}", ktxErrorString(result));
        }
      } else {
        // Use the format that the image is already in
        rawImage.format_ktx = vuk::Format(VkFormat(ktx->vkFormat));
      }

      rawImage.width1 = ktx->baseWidth;
      rawImage.height = ktx->baseHeight;
      rawImage.ktx.reset(ktx);
    } else {
      OX_SCOPED_ZONE_N("Decode JPEG/PNG");
      int x, y, comp;
      auto* pixels = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(rawImage.encoded_pixel_data.get()),
                                           static_cast<int>(rawImage.encoded_pixel_size),
                                           &x,
                                           &y,
                                           &comp,
                                           4);

      OX_ASSERT(pixels != nullptr);

      rawImage.width1 = x;
      rawImage.height = y;
      rawImage.data.reset(pixels);
    }

    return rawImage;
  });

  // Upload image data to GPU
  std::vector<Shared<Texture>> loaded_images = {};
  loaded_images.reserve(raw_image_datas.size());

  for (const auto& image : raw_image_datas) {
    const vuk::Extent3D dims = {static_cast<uint32>(image.width1), static_cast<uint32>(image.height), 1u};

    const auto* ktx = image.ktx.get();

    auto ci = TextureLoadInfo{
      .path = {},
      .preset = Preset::eMap2D,
      .extent = dims,
      .format = vuk::Format::eR8G8B8A8Unorm,
      .data = image.is_ktx ? ktx->kvData : image.data.get(),
      .mime = image.is_ktx ? TextureLoadInfo::MimeType::KTX : TextureLoadInfo::MimeType::Generic,
    };

    const auto& t = loaded_images.emplace_back(AssetManager::get_texture_asset(image.name, ci));
    t->set_name(image.name);
  }

  return loaded_images;
}

std::vector<Shared<Material>> Mesh::load_materials(const fastgltf::Asset& asset, const std::vector<Shared<Texture>>& images) {
  OX_SCOPED_ZONE;

  std::vector<Shared<Material>> materials = {};
  materials.reserve(asset.materials.size());

  if (asset.materials.empty()) {
    const auto& ma = materials.emplace_back(AssetManager::get_material_asset("placeholder"));
    ma->create();
  }

  for (auto& material : asset.materials) {
    const Shared<Material>& ma = materials.emplace_back(AssetManager::get_material_asset(material.name.c_str()));
    ma->create();

    if (material.pbrData.baseColorTexture.has_value()) {
      const auto& base_color_texture = asset.textures[material.pbrData.baseColorTexture->textureIndex];

      auto& image = images[base_color_texture.imageIndex.value()];
      ma->set_albedo_texture(image);

      // extract sampler
      if (!asset.samplers.empty()) {
        const auto extract_sampler = [](const fastgltf::Sampler& sampler) -> Material::Sampler {
          switch (sampler.magFilter.value_or(fastgltf::Filter::Linear)) {
            case fastgltf::Filter::Nearest:
            case fastgltf::Filter::NearestMipMapNearest:
            case fastgltf::Filter::NearestMipMapLinear : return Material::Sampler::Nearest;
            case fastgltf::Filter::Linear              :
            case fastgltf::Filter::LinearMipMapNearest :
            case fastgltf::Filter::LinearMipMapLinear  :
            default                                    : return Material::Sampler::Anisotropy;
          }
        };

        const auto sampler = extract_sampler(asset.samplers.at(base_color_texture.samplerIndex.value_or(fastgltf::Filter::Linear)));
        ma->set_sampler(sampler);
      }
    }

    if (material.occlusionTexture.has_value()) {
      const auto& occlusion_texture = asset.textures[material.occlusionTexture->textureIndex];
      auto& image = images[occlusion_texture.imageIndex.value()];
      ma->set_ao_texture(image);
    }

    if (material.emissiveTexture.has_value()) {
      const auto& emissive_texture = asset.textures[material.emissiveTexture->textureIndex];
      auto& image = images[emissive_texture.imageIndex.value()];
      ma->set_emissive_texture(image);
    }

    if (material.normalTexture.has_value()) {
      const auto& normal_texture = asset.textures[material.normalTexture->textureIndex];
      auto& image = images[normal_texture.imageIndex.value()];
      ma->set_normal_texture(image);
      // ma->normalXyScale = material.normalTexture->scale; TODO:
    }

    if (material.pbrData.metallicRoughnessTexture.has_value()) {
      const auto& metallic_roughness_texture = asset.textures[material.pbrData.metallicRoughnessTexture->textureIndex];
      auto& image = images[metallic_roughness_texture.imageIndex.value()];
      ma->set_physical_texture(image);
    }

    // TODO:
    // - Anisotropy
    // - Clearcoat
    // - Sheen

    ma->set_color(glm::make_vec4(material.pbrData.baseColorFactor.data()))
      ->set_metallic(material.pbrData.metallicFactor)
      ->set_roughness(material.pbrData.roughnessFactor)
      ->set_emissive(float4(glm::make_vec3(material.emissiveFactor.data()), material.emissiveStrength))
      ->set_reflectance(0.04f)
      ->set_double_sided(material.doubleSided)
      ->set_alpha_mode((Material::AlphaMode)(uint32)material.alphaMode)
      ->set_alpha_cutoff(material.alphaCutoff);
  }

  return materials;
}
} // namespace ox
