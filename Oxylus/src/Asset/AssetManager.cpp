#include "AssetManager.hpp"

#include "Thread/TaskScheduler.hpp"

namespace ox {

void AssetManager::init() {}

void AssetManager::deinit() {}

auto AssetManager::registry() const -> const AssetRegistry& { return asset_registry; }

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

} // namespace ox
