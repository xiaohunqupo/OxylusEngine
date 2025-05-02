#include "AssetManager.hpp"

namespace ox {

void AssetManager::init() {}

void AssetManager::deinit() {}

auto AssetManager::registry() const -> const AssetRegistry& { return asset_registry; }

auto AssetManager::load_asset(const UUID& uuid) -> bool {
  auto* asset = this->get_asset(uuid);
  switch (asset->type) {
    case AssetType::Model: {
      return this->load_model(uuid);
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
    case AssetType::Model: {
      this->unload_model(uuid);
    } break;
    case AssetType::Texture: {
      this->unload_texture(uuid);
    } break;
    case AssetType::Scene: {
      this->unload_scene(uuid);
    } break;
    default:;
  }
}

auto AssetManager::load_model(const UUID& uuid) -> bool {
  OX_SCOPED_ZONE;

  OX_UNIMPLEMENTED(AssetManager::load_model);

  return false;
}

auto AssetManager::unload_model(const UUID& uuid) -> void {
  OX_SCOPED_ZONE;

  OX_UNIMPLEMENTED(AssetManager::unload_model);
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

  OX_UNIMPLEMENTED(AssetManager::load_material);

  return false;
}

auto AssetManager::unload_material(const UUID& uuid) -> void {
  OX_SCOPED_ZONE;

  OX_UNIMPLEMENTED(AssetManager::unload_material);
}

auto AssetManager::load_scene(const UUID& uuid) -> bool {
  OX_SCOPED_ZONE;

  OX_UNIMPLEMENTED(AssetManager::load_scene);

  return false;
}

auto AssetManager::unload_scene(const UUID& uuid) -> void {
  OX_SCOPED_ZONE;

  OX_UNIMPLEMENTED(AssetManager::unload_scene);
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

  OX_LOG_TRACE("Unloaded audio {}.", uuid.str());

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
} // namespace ox
