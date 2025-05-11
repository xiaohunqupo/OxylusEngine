#include "AssetManager.hpp"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "Asset/AssetFile.hpp"
#include "Asset/Material.hpp"
#include "Core/FileSystem.hpp"
#include "Memory/Hasher.hpp"
#include "Memory/Stack.hpp"
#include "Scripting/LuaSystem.hpp"
#include "Thread/TaskScheduler.hpp"
#include "Utils/JsonHelpers.hpp"
#include "Utils/Log.hpp"
#include "Utils/Profiler.hpp"

namespace ox {

auto AssetManager::init() -> std::expected<void,
                                           std::string> {
  return {};
}

auto AssetManager::deinit() -> std::expected<void,
                                             std::string> {
  return {};
}

auto AssetManager::registry() const -> const AssetRegistry& { return asset_registry; }

auto begin_asset_meta(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                      const UUID& uuid,
                      AssetType type) -> void {
  OX_SCOPED_ZONE;

  writer.StartObject();

  writer.String("uuid");
  writer.String(uuid.str().c_str());

  writer.String("type");
  writer.Uint(std::to_underlying(type));
}

auto write_texture_asset_meta(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                              Texture*) -> bool {
  OX_SCOPED_ZONE;

  return true;
}

auto write_material_asset_meta(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                               const UUID& uuid,
                               const Material& material) -> bool {
  OX_SCOPED_ZONE;

  writer.String("material");
  writer.StartObject();

  writer.String("uuid");
  writer.String(uuid.str().c_str());

  writer.String("albedo_color");
  serialize_vec4(writer, material.albedo_color);

  writer.String("emissive_color");
  serialize_vec3(writer, material.emissive_color);

  writer.String("roughness_factor");
  writer.Double(material.roughness_factor);

  writer.String("metallic_factor");
  writer.Double(material.metallic_factor);

  writer.String("alpha_mode");
  writer.Uint(std::to_underlying(material.alpha_mode));

  writer.String("alpha_cutoff");
  writer.Double(material.alpha_cutoff);

  writer.String("albedo_texture");
  writer.String(material.albedo_texture.str().c_str());

  writer.String("normal_texture");
  writer.String(material.normal_texture.str().c_str());

  writer.String("emissive_texture");
  writer.String(material.emissive_texture.str().c_str());

  writer.String("metallic_roughness_texture");
  writer.String(material.metallic_roughness_texture.str().c_str());

  writer.String("occlusion_texture");
  writer.String(material.occlusion_texture.str().c_str());

  writer.EndObject();

  return true;
}

auto read_material_asset_meta(const rapidjson::Document& doc,
                              Material* mat) -> bool {
  OX_SCOPED_ZONE;

  if (!mat)
    return false;

  const auto& material_obj = doc["material"].GetObject();

  deserialize_vec4(material_obj["albedo_color"].GetArray(), &mat->albedo_color);
  deserialize_vec3(material_obj["emissive_color"].GetArray(), &mat->emissive_color);
  mat->roughness_factor = material_obj["roughness_factor"].GetDouble();
  mat->metallic_factor = material_obj["metallic_factor"].GetFloat();
  mat->alpha_mode = static_cast<AlphaMode>(material_obj["alpha_mode"].GetUint());
  mat->alpha_cutoff = material_obj["alpha_cutoff"].GetFloat();
  mat->albedo_texture = UUID::from_string(material_obj["albedo_texture"].GetString()).value_or(UUID(nullptr));
  mat->normal_texture = UUID::from_string(material_obj["normal_texture"].GetString()).value_or(UUID(nullptr));
  mat->emissive_texture = UUID::from_string(material_obj["emissive_texture"].GetString()).value_or(UUID(nullptr));
  mat->metallic_roughness_texture =
      UUID::from_string(material_obj["metallic_roughness_texture"].GetString()).value_or(UUID(nullptr));
  mat->occlusion_texture = UUID::from_string(material_obj["occlusion_texture"].GetString()).value_or(UUID(nullptr));

  return true;
}

auto write_mesh_asset_meta(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                           std::span<UUID> embedded_texture_uuids,
                           std::span<UUID> material_uuids,
                           std::span<Material> materials) -> bool {
  OX_SCOPED_ZONE;

  writer.String("embedded_textures");
  writer.StartArray();
  for (const auto& uuid : embedded_texture_uuids) {
    writer.String(uuid.str().c_str());
  }
  writer.EndArray();

  writer.String("embedded_materials");
  writer.StartArray();
  for (const auto& [material_uuid, material] : std::views::zip(material_uuids, materials)) {
    write_material_asset_meta(writer, material_uuid, material);
  }
  writer.EndArray();

  return true;
}

auto write_scene_asset_meta(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                            Scene* scene) -> bool {
  OX_SCOPED_ZONE;

  writer.String("name");
  writer.String(scene->scene_name.c_str());

  return true;
}

auto write_script_asset_meta(rapidjson::PrettyWriter<rapidjson::StringBuffer>&,
                             LuaSystem*) -> bool {
  OX_SCOPED_ZONE;

  return true;
}

auto end_asset_meta(rapidjson::StringBuffer& sb,
                    rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                    const std::string& path) -> bool {
  ZoneScoped;

  writer.EndObject();

  auto meta_path = path + ".oxasset";

  std::ofstream filestream(meta_path);
  filestream << sb.GetString();

  return true;
}

auto AssetManager::load_deferred_assets() -> void {
  for (auto& task : deferred_load_queue) {
    task();
  }
  deferred_load_queue.clear();
}

auto AssetManager::to_asset_file_type(const std::string& path) -> AssetFileType {
  ZoneScoped;
  memory::ScopedStack stack;

  auto extension = ox::fs::get_file_extension(path);

  if (extension.empty()) {
    return AssetFileType::None;
  }

  extension = stack.to_upper(extension);
  switch (fnv64_str(extension)) {
    case fnv64_c("GLB")    : return AssetFileType::GLB;
    case fnv64_c("GLTF")   : return AssetFileType::GLTF;
    case fnv64_c("PNG")    : return AssetFileType::PNG;
    case fnv64_c("JPG")    :
    case fnv64_c("JPEG")   : return AssetFileType::JPEG;
    case fnv64_c("JSON")   : return AssetFileType::JSON;
    case fnv64_c("OXASSET"): return AssetFileType::Meta;
    case fnv64_c("KTX2")   : return AssetFileType::KTX2;
    case fnv64_c("LUA")    : return AssetFileType::LUA;
    default                : return AssetFileType::None;
  }
}

auto AssetManager::to_asset_type_sv(AssetType type) -> std::string_view {
  ZoneScoped;

  switch (type) {
    case AssetType::None    : return "None";
    case AssetType::Shader  : return "Shader";
    case AssetType::Mesh    : return "Mesh";
    case AssetType::Texture : return "Texture";
    case AssetType::Material: return "Material";
    case AssetType::Font    : return "Font";
    case AssetType::Scene   : return "Scene";
    case AssetType::Audio   : return "Audio";
    case AssetType::Script  : return "Script";
  }
}

auto AssetManager::create_asset(const AssetType type,
                                const std::string& path) -> UUID {
  const auto uuid = UUID::generate_random();
  auto [asset_it, inserted] = asset_registry.try_emplace(uuid);
  if (!inserted) {
    OX_LOG_ERROR("Can't create asset {}!", uuid.str());
    return UUID(nullptr);
  }

  auto& asset = asset_it->second;
  asset.uuid = uuid;
  asset.type = type;
  asset.path = path;

  return asset.uuid;
}

auto AssetManager::import_asset(const std::string& path) -> UUID {
  OX_SCOPED_ZONE;
  memory::ScopedStack stack;

  if (!fs::exists(path)) {
    OX_LOG_ERROR("Trying to import an asset '{}' that doesn't exist.", path);
    return UUID(nullptr);
  }

  auto asset_type = AssetType::None;
  switch (this->to_asset_file_type(path)) {
    case AssetFileType::Meta: {
      return this->register_asset(path);
    }
    case AssetFileType::GLB:
    case AssetFileType::GLTF: {
      asset_type = AssetType::Mesh;
      break;
    }
    case AssetFileType::PNG:
    case AssetFileType::JPEG:
    case AssetFileType::KTX2: {
      asset_type = AssetType::Texture;
      break;
    }
    case ox::AssetFileType::LUA: {
      asset_type = AssetType::Script;
      break;
    }
    default: {
      return UUID(nullptr);
    }
  }

  // Check for meta file before creating new asset
  auto meta_path = stack.format("{}.oxasset", path);
  if (fs::exists(meta_path)) {
    return this->register_asset(std::string(meta_path));
  }

  auto uuid = this->create_asset(asset_type, path);
  if (!uuid) {
    return UUID(nullptr);
  }

  rapidjson::StringBuffer sb;

  rapidjson::PrettyWriter writer(sb);
  begin_asset_meta(writer, uuid, asset_type);

  switch (asset_type) {
    case AssetType::Mesh: {
#if 0 // TODO:
      auto gltf_model = GLTFMeshInfo::parse_info(path);
      auto textures = std::vector<UUID>();
      auto embedded_textures = std::vector<UUID>();
      for (auto& v : gltf_model->textures) {
        auto& image = gltf_model->images[v.image_index.value()];
        auto& texture_uuid = textures.emplace_back();
        auto match = ox::match{
            [&](const std::vector<u8>&) {
          texture_uuid = this->create_asset(AssetType::Texture, path);
          embedded_textures.push_back(texture_uuid);
        },
            [&](const std::string& image_path) { //
          texture_uuid = this->import_asset(image_path);
        },
        };
        std::visit(match, image.image_data);
      }

      auto material_uuids = std::vector<UUID>(gltf_model->materials.size());
      auto materials = std::vector<Material>(gltf_model->materials.size());
      for (const auto& [material_uuid, material, gltf_material] :
           std::views::zip(material_uuids, materials, gltf_model->materials)) {
        material_uuid = this->create_asset(AssetType::Material);
        material.albedo_color = gltf_material.albedo_color;
        material.emissive_color = gltf_material.emissive_color;
        material.roughness_factor = gltf_material.roughness_factor;
        material.metallic_factor = gltf_material.metallic_factor;
        material.alpha_mode = static_cast<AlphaMode>(gltf_material.alpha_mode);
        material.alpha_cutoff = gltf_material.alpha_cutoff;

        if (auto tex_idx = gltf_material.albedo_texture_index; tex_idx.has_value()) {
          material.albedo_texture = textures[tex_idx.value()];
        }

        if (auto tex_idx = gltf_material.normal_texture_index; tex_idx.has_value()) {
          material.normal_texture = textures[tex_idx.value()];
        }

        if (auto tex_idx = gltf_material.emissive_texture_index; tex_idx.has_value()) {
          material.emissive_texture = textures[tex_idx.value()];
        }

        if (auto tex_idx = gltf_material.metallic_roughness_texture_index; tex_idx.has_value()) {
          material.metallic_roughness_texture = textures[tex_idx.value()];
        }

        if (auto tex_idx = gltf_material.occlusion_texture_index; tex_idx.has_value()) {
          material.occlusion_texture = textures[tex_idx.value()];
        }
      }

      write_mesh_asset_meta(writer, embedded_textures, material_uuids, materials);
#endif
    } break;
    case AssetType::Texture: {
      Texture texture = {};

      write_texture_asset_meta(writer, &texture);
    } break;
    case ox::AssetType::Script: {
      write_script_asset_meta(writer, nullptr);
      break;
    }
    default:;
  }

  if (!end_asset_meta(sb, writer, path)) {
    return UUID(nullptr);
  }

  return uuid;
}

auto AssetManager::delete_asset(const UUID& uuid) -> void {
  OX_SCOPED_ZONE;

  auto* asset = this->get_asset(uuid);
  if (asset->ref_count > 0) {
    OX_LOG_WARN("Deleting alive asset {} with {} references!", asset->uuid.str(), asset->ref_count);
  }

  if (asset->is_loaded()) {
    asset->ref_count = ox::min(asset->ref_count, 1_u64);
    this->unload_asset(uuid);
    asset_registry.erase(uuid);
  }

  OX_LOG_TRACE("Deleted asset {}.", uuid.str());
}

auto AssetManager::register_asset(const std::string& path) -> UUID {
  OX_SCOPED_ZONE;
  memory::ScopedStack stack;

  const auto content = ox::fs::read_file(path);
  if (content.empty()) {
    OX_LOG_ERROR("Failed to open file {}!", path);
    return UUID(nullptr);
  }

  rapidjson::Document doc;
  doc.Parse(content.data());

  const rapidjson::ParseResult parse_result = doc.Parse(content.c_str());

  if (doc.HasParseError()) {
    OX_LOG_ERROR("Json parser error for: {0} {1}", path, rapidjson::GetParseError_En(parse_result.Code()));
    return UUID(nullptr);
  }

  auto uuid_json = doc["uuid"].GetString();

  auto type_json = doc["type"].GetUint();

  auto asset_path = ::fs::path(path);
  asset_path.replace_extension("");
  auto uuid = UUID::from_string(uuid_json).value();
  auto type = static_cast<AssetType>(type_json);

  if (!this->register_asset(uuid, type, asset_path)) {
    return UUID(nullptr);
  }

  switch (type) {
    case AssetType::Material: {
      Material mat = {};
      if (read_material_asset_meta(doc, &mat)) {
        /* Since materials could contain textures that may be not yet registered
           we defer them to be loaded at the end of frame */
        deferred_load_queue.emplace_back([this, uuid, mat]() { load_material(uuid, mat); });
      } else {
        OX_LOG_ERROR("Couldn't parse material meta data!");
      }
    } break;
  }

  return uuid;
}

auto AssetManager::register_asset(const UUID& uuid,
                                  AssetType type,
                                  const std::string& path) -> bool {
  auto [asset_it, inserted] = asset_registry.try_emplace(uuid);
  if (!inserted) {
    if (asset_it != asset_registry.end()) {
      // Tried a reinsert, asset already exists
      return true;
    }

    return false;
  }

  auto& asset = asset_it->second;
  asset.uuid = uuid;
  asset.path = path;
  asset.type = type;

  OX_LOG_INFO("Registered new asset: {}", uuid.str());

  return true;
}

auto AssetManager::export_asset(const UUID& uuid,
                                const std::string& path) -> bool {
  auto* asset = this->get_asset(uuid);

  rapidjson::StringBuffer sb;

  rapidjson::PrettyWriter writer(sb);

  begin_asset_meta(writer, uuid, asset->type);

  switch (asset->type) {
    case AssetType::Texture: {
      if (!this->export_texture(asset->uuid, writer, path)) {
        return false;
      }
    } break;
    case AssetType::Mesh: {
      if (!this->export_mesh(asset->uuid, writer, path)) {
        return false;
      }
    } break;
    case AssetType::Scene: {
      if (!this->export_scene(asset->uuid, writer, path)) {
        return false;
      }
    } break;
    case ox::AssetType::Material: {
      if (!this->export_material(asset->uuid, writer, path)) {
        return false;
      }
    } break;
    case ox::AssetType::Script: {
      if (!this->export_script(asset->uuid, writer, path)) {
        return false;
      }
    } break;
    default: return false;
  }

  return end_asset_meta(sb, writer, path);
}

auto AssetManager::export_texture(const UUID& uuid,
                                  rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                                  const std::string& path) -> bool {
  OX_SCOPED_ZONE;

  auto* texture = this->get_texture(uuid);
  OX_CHECK_NULL(texture);
  return write_texture_asset_meta(writer, texture);
}

auto AssetManager::export_mesh(const UUID& uuid,
                               rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                               const std::string& path) -> bool {
  OX_SCOPED_ZONE;

  OX_UNIMPLEMENTED(AssetManager::export_mesh);

  return false;
#if 0
  auto materials = std::vector<Material>(mesh->materials.size());
  for (const auto& [material_uuid, material] : std::views::zip(model->materials, materials)) {
    material = *this->get_material(material_uuid);
  }

  return write_mesh_asset_meta(json, model->embedded_textures, model->materials, materials);
#endif
}

auto AssetManager::export_scene(const UUID& uuid,
                                rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                                const std::string& path) -> bool {
  OX_SCOPED_ZONE;

  auto* scene = this->get_scene(uuid);
  OX_CHECK_NULL(scene);
  write_scene_asset_meta(writer, scene);

  return scene->save_to_file(path);
}

auto AssetManager::export_material(const UUID& uuid,
                                   rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                                   const std::string& path) -> bool {
  OX_SCOPED_ZONE;

  auto* material = this->get_material(uuid);
  OX_CHECK_NULL(material);
  return write_material_asset_meta(writer, uuid, *material);
}

auto AssetManager::export_script(const UUID& uuid,
                                 rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                                 const std::string& path) -> bool {
  OX_SCOPED_ZONE;

  return write_texture_asset_meta(writer, nullptr);
}

auto AssetManager::load_asset(const UUID& uuid) -> bool {
  const auto* asset = this->get_asset(uuid);
  switch (asset->type) {
    case AssetType::Mesh: {
      return this->load_mesh(uuid);
    }
    case AssetType::Texture: {
      return this->load_texture(uuid);
    }
    case AssetType::Scene: {
      return this->load_scene(uuid);
    }
    case AssetType::Audio: {
      return this->load_audio(uuid);
    }
    default:;
  }

  return false;
}

auto AssetManager::unload_asset(const UUID& uuid) -> void {
  const auto* asset = this->get_asset(uuid);
  OX_CHECK_NULL(asset);
  switch (asset->type) {
    case AssetType::Mesh: {
      this->unload_mesh(uuid);
    } break;
    case AssetType::Texture: {
      this->unload_texture(uuid);
    } break;
    case AssetType::Scene: {
      this->unload_scene(uuid);
    } break;
    case AssetType::Audio: {
      this->unload_audio(uuid);
      break;
    }
    default:;
  }
}

auto AssetManager::load_mesh(const UUID& uuid) -> bool {
  OX_SCOPED_ZONE;

  OX_UNIMPLEMENTED(AssetManager::load_mesh);

  return false;
}

auto AssetManager::unload_mesh(const UUID& uuid) -> void {
  OX_SCOPED_ZONE;

  OX_UNIMPLEMENTED(AssetManager::unload_mesh);
}

auto AssetManager::load_texture(const UUID& uuid,
                                const TextureLoadInfo& info) -> bool {
  OX_SCOPED_ZONE;

  auto* asset = this->get_asset(uuid);
  OX_CHECK_NULL(asset);
  asset->acquire_ref();

  if (asset->is_loaded()) {
    return true;
  }

  textures_mutex.lock();

  Texture texture{};
  texture.create(asset->path, info);
  asset->texture_id = texture_map.create_slot(std::move(texture));

  textures_mutex.unlock();

  OX_LOG_INFO("Loaded texture {} {}.", asset->uuid.str(), SlotMap_decode_id(asset->texture_id).index);

  return true;
}

auto AssetManager::unload_texture(const UUID& uuid) -> void {
  OX_SCOPED_ZONE;

  auto* asset = this->get_asset(uuid);
  if (!asset || !(asset->is_loaded() && asset->release_ref())) {
    return;
  }

  OX_LOG_TRACE("Unloaded texture {}.", uuid.str());

  texture_map.destroy_slot(asset->texture_id);
  asset->texture_id = TextureID::Invalid;
}

auto AssetManager::load_material(const UUID& uuid,
                                 const Material& material_info) -> bool {
  OX_SCOPED_ZONE;

  auto* asset = this->get_asset(uuid);
  OX_CHECK_NULL(asset);

  // Materials don't explicitly load any resources, they need to increase child resources refs.

  if (!asset->is_loaded()) {
    asset->material_id = material_map.create_slot(const_cast<Material&&>(material_info));
  }

  struct TextureLoadTask : ITaskSet {
    const std::vector<UUID>& uuids;
    const std::vector<TextureLoadInfo>& load_infos;
    AssetManager& asset_manager;

    TextureLoadTask(const std::vector<UUID>& uuid,
                    const std::vector<TextureLoadInfo>& load_info,
                    AssetManager& asset_man) :
        uuids(uuid),
        load_infos(load_info),
        asset_manager(asset_man) {
      this->m_SetSize = static_cast<uint32_t>(uuids.size()); // One task per texture
    }

    void ExecuteRange(const enki::TaskSetPartition range,
                      uint32_t threadNum) override {
      for (uint32_t i = range.start; i < range.end; ++i) {
        asset_manager.load_texture(uuids[i], load_infos[i]);
      }
    }
  };

  std::vector<UUID> texture_uuids = {};
  std::vector<TextureLoadInfo> load_infos = {};

  auto* material = material_map.slot(asset->material_id);

  if (material->albedo_texture) {
    texture_uuids.emplace_back(material->albedo_texture);
    load_infos.emplace_back(TextureLoadInfo{.format = vuk::Format::eR8G8B8A8Srgb});
  }

  if (material->normal_texture) {
    texture_uuids.emplace_back(material->normal_texture);
    load_infos.emplace_back(TextureLoadInfo{.format = vuk::Format::eR8G8B8A8Unorm});
  }

  if (material->emissive_texture) {
    texture_uuids.emplace_back(material->emissive_texture);
    load_infos.emplace_back(TextureLoadInfo{.format = vuk::Format::eR8G8B8A8Srgb});
  }

  if (material->metallic_roughness_texture) {
    texture_uuids.emplace_back(material->metallic_roughness_texture);
    load_infos.emplace_back(TextureLoadInfo{.format = vuk::Format::eR8G8B8A8Unorm});
  }

  if (material->occlusion_texture) {
    texture_uuids.emplace_back(material->metallic_roughness_texture);
    load_infos.emplace_back(TextureLoadInfo{.format = vuk::Format::eR8G8B8A8Unorm});
  }

  auto load_task = TextureLoadTask(texture_uuids, load_infos, *this);

  const auto* task_scheduler = App::get_system<TaskScheduler>(EngineSystems::TaskScheduler);
  task_scheduler->schedule_task(&load_task);
  task_scheduler->wait_task(&load_task);

  asset->acquire_ref();
  return true;
}

auto AssetManager::unload_material(const UUID& uuid) -> void {
  OX_SCOPED_ZONE;

  auto* asset = this->get_asset(uuid);
  OX_CHECK_NULL(asset);
  if (!(asset->is_loaded() && asset->release_ref())) {
    return;
  }

  const auto* material = this->get_material(asset->material_id);
  if (material->albedo_texture) {
    this->unload_texture(material->albedo_texture);
  }

  if (material->normal_texture) {
    this->unload_texture(material->normal_texture);
  }

  if (material->emissive_texture) {
    this->unload_texture(material->emissive_texture);
  }

  if (material->metallic_roughness_texture) {
    this->unload_texture(material->metallic_roughness_texture);
  }

  if (material->occlusion_texture) {
    this->unload_texture(material->occlusion_texture);
  }

  material_map.destroy_slot(asset->material_id);
  asset->material_id = MaterialID::Invalid;
}

auto AssetManager::load_scene(const UUID& uuid) -> bool {
  OX_SCOPED_ZONE;

  auto* asset = this->get_asset(uuid);
  asset->scene_id = this->scene_map.create_slot(std::make_unique<Scene>());
  auto* scene = this->scene_map.slot(asset->scene_id)->get();

  scene->init("unnamed_scene");

  if (!scene->load_from_file(asset->path)) {
    return false;
  }

  asset->acquire_ref();
  return true;
}

auto AssetManager::unload_scene(const UUID& uuid) -> void {
  OX_SCOPED_ZONE;

  auto* asset = this->get_asset(uuid);
  OX_CHECK_NULL(asset);
  if (!(asset->is_loaded() && asset->release_ref())) {
    return;
  }

  scene_map.destroy_slot(asset->scene_id);
  asset->scene_id = SceneID::Invalid;
}

auto AssetManager::load_audio(const UUID& uuid) -> bool {
  OX_SCOPED_ZONE;

  auto* asset = this->get_asset(uuid);
  OX_CHECK_NULL(asset);
  asset->acquire_ref();

  if (asset->is_loaded()) {
    return true;
  }

  AudioSource audio{};
  audio.load(asset->path);
  asset->audio_id = audio_map.create_slot(std::move(audio));

  OX_LOG_INFO("Loaded audio {} {}.", asset->uuid.str(), SlotMap_decode_id(asset->audio_id).index);

  return true;
}

auto AssetManager::unload_audio(const UUID& uuid) -> void {
  OX_SCOPED_ZONE;

  auto* asset = this->get_asset(uuid);
  if (!asset || !(asset->is_loaded() && asset->release_ref())) {
    return;
  }

  auto* audio = this->get_audio(asset->audio_id);
  OX_CHECK_NULL(audio);
  audio->unload();

  OX_LOG_INFO("Unloaded audio {}.", uuid.str());

  audio_map.destroy_slot(asset->audio_id);
  asset->audio_id = AudioID::Invalid;
}

auto AssetManager::load_script(const UUID& uuid) -> bool {
  OX_SCOPED_ZONE;

  auto* asset = this->get_asset(uuid);
  OX_CHECK_NULL(asset);
  asset->acquire_ref();

  if (asset->is_loaded())
    return true;

  asset->script_id = script_map.create_slot(std::make_unique<LuaSystem>());
  auto* system = script_map.slot(asset->script_id);
  system->get()->load(asset->path);

  OX_LOG_INFO("Loaded script {} {}.", asset->uuid.str(), SlotMap_decode_id(asset->script_id).index);

  return true;
}

auto AssetManager::unload_script(const UUID& uuid) -> void {
  OX_SCOPED_ZONE;

  auto* asset = this->get_asset(uuid);
  if (!asset || !(asset->is_loaded() && asset->release_ref())) {
    return;
  }

  script_map.destroy_slot(asset->script_id);
  asset->script_id = ScriptID::Invalid;

  OX_LOG_INFO("Unloaded script {}.", uuid.str());
}

auto AssetManager::get_asset(const UUID& uuid) -> Asset* {
  const auto it = asset_registry.find(uuid);
  if (it == asset_registry.end()) {
    return nullptr;
  }

  return &it->second;
}

auto AssetManager::get_mesh(const UUID& uuid) -> Mesh* {
  OX_SCOPED_ZONE;

  const auto* asset = this->get_asset(uuid);
  if (asset == nullptr) {
    return nullptr;
  }

  OX_CHECK_EQ(asset->type, AssetType::Mesh);
  if (asset->type != AssetType::Mesh || asset->mesh_id == MeshID::Invalid) {
    return nullptr;
  }

  return mesh_map.slot(asset->mesh_id);
}

auto AssetManager::get_mesh(const MeshID mesh_id) -> Mesh* {
  OX_SCOPED_ZONE;

  if (mesh_id == MeshID::Invalid) {
    return nullptr;
  }

  return mesh_map.slot(mesh_id);
}

auto AssetManager::get_texture(const UUID& uuid) -> Texture* {
  OX_SCOPED_ZONE;

  const auto* asset = this->get_asset(uuid);
  if (asset == nullptr) {
    return nullptr;
  }

  OX_CHECK_EQ(asset->type, AssetType::Texture);
  if (asset->type != AssetType::Texture || asset->texture_id == TextureID::Invalid) {
    return nullptr;
  }

  return texture_map.slot(asset->texture_id);
}

auto AssetManager::get_texture(const TextureID texture_id) -> Texture* {
  OX_SCOPED_ZONE;

  if (texture_id == TextureID::Invalid) {
    return nullptr;
  }

  return texture_map.slot(texture_id);
}

auto AssetManager::get_material(const UUID& uuid) -> Material* {
  OX_SCOPED_ZONE;

  const auto* asset = this->get_asset(uuid);
  if (asset == nullptr) {
    return nullptr;
  }

  OX_CHECK_EQ(asset->type, AssetType::Material);
  if (asset->type != AssetType::Material || asset->material_id == MaterialID::Invalid) {
    return nullptr;
  }

  return material_map.slot(asset->material_id);
}

auto AssetManager::get_material(const MaterialID material_id) -> Material* {
  OX_SCOPED_ZONE;

  if (material_id == MaterialID::Invalid) {
    return nullptr;
  }

  return material_map.slot(material_id);
}

auto AssetManager::get_scene(const UUID& uuid) -> Scene* {
  OX_SCOPED_ZONE;

  const auto* asset = this->get_asset(uuid);
  if (asset == nullptr) {
    return nullptr;
  }

  OX_CHECK_EQ(asset->type, AssetType::Scene);
  if (asset->type != AssetType::Scene || asset->scene_id == SceneID::Invalid) {
    return nullptr;
  }

  return scene_map.slot(asset->scene_id)->get();
}

auto AssetManager::get_scene(const SceneID scene_id) -> Scene* {
  OX_SCOPED_ZONE;

  if (scene_id == SceneID::Invalid) {
    return nullptr;
  }

  return scene_map.slot(scene_id)->get();
}

auto AssetManager::get_audio(const UUID& uuid) -> AudioSource* {
  const auto* asset = this->get_asset(uuid);
  if (asset == nullptr) {
    return nullptr;
  }

  OX_CHECK_EQ(asset->type, AssetType::Audio);
  if (asset->type != AssetType::Audio || asset->audio_id == AudioID::Invalid) {
    return nullptr;
  }

  return audio_map.slot(asset->audio_id);
}

auto AssetManager::get_audio(const AudioID audio_id) -> AudioSource* {
  OX_SCOPED_ZONE;

  if (audio_id == AudioID::Invalid) {
    return nullptr;
  }

  return audio_map.slot(audio_id);
}

auto AssetManager::get_script(const UUID& uuid) -> LuaSystem* {
  const auto* asset = this->get_asset(uuid);
  if (asset == nullptr) {
    return nullptr;
  }

  OX_CHECK_EQ(asset->type, AssetType::Script);
  if (asset->type != AssetType::Script || asset->audio_id == AudioID::Invalid) {
    return nullptr;
  }

  return script_map.slot(asset->script_id)->get();
}

auto AssetManager::get_script(ScriptID script_id) -> LuaSystem* {
  OX_SCOPED_ZONE;

  if (script_id == ScriptID::Invalid) {
    return nullptr;
  }

  return script_map.slot(script_id)->get();
}
} // namespace ox
