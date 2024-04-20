#include "AssetManager.hpp"

#include <filesystem>

#include "Audio/AudioSource.hpp"
#include "Render/Mesh.hpp"

#include "Utils/Log.hpp"
#include "Utils/Profiler.hpp"

namespace ox {
AssetManager::State AssetManager::_state;

Shared<Texture> AssetManager::get_texture_asset(const TextureLoadInfo& info) {
  if (_state.texture_assets.contains(info.path)) {
    return _state.texture_assets[info.path];
  }

  return load_texture_asset(info.path, info);
}

Shared<Texture> AssetManager::get_texture_asset(const std::string& name, const TextureLoadInfo& info) {
  if (_state.texture_assets.contains(name)) {
    return _state.texture_assets[name];
  }

  return load_texture_asset(name, info);
}

AssetTask<Texture>* AssetManager::get_texture_asset_future(const TextureLoadInfo& info) {
  const auto* t = &_state.texture_tasks.emplace_back(create_unique<AssetTask<Texture>>([info] {
    if (_state.texture_assets.contains(info.path)) {
      return _state.texture_assets[info.path];
    }

    return load_texture_asset(info.path, info);
  }));

  return t->get();
}

Shared<Mesh> AssetManager::get_mesh_asset(const std::string& path, const uint32_t loadingFlags) {
  OX_SCOPED_ZONE;
  if (_state.mesh_assets.contains(path)) {
    return _state.mesh_assets[path];
  }

  return load_mesh_asset(path, loadingFlags);
}

AssetTask<Mesh>* AssetManager::get_mesh_asset_future(const std::string& path, uint32_t loadingFlags) {
  const auto* t = &_state.mesh_tasks.emplace_back(create_unique<AssetTask<Mesh>>([path, loadingFlags] {
    if (_state.mesh_assets.contains(path)) {
      return _state.mesh_assets[path];
    }

    return load_mesh_asset(path, loadingFlags);
  }));

  return t->get();
}

Shared<Material> AssetManager::get_material_asset(const std::string& name) {
  const auto& ma = _state.material_assets.emplace_back(create_shared<Material>(name));
  ma->set_id((uint32)_state.material_assets.size() - 1);
  return ma;
}

Shared<AudioSource> AssetManager::get_audio_asset(const std::string& path) {
  OX_SCOPED_ZONE;
  if (_state.audio_assets.contains(path)) {
    return _state.audio_assets[path];
  }

  return load_audio_asset(path);
}

Shared<Texture> AssetManager::load_texture_asset(const std::string& path, const TextureLoadInfo& info) {
  OX_SCOPED_ZONE;

  Shared<Texture> texture = create_shared<Texture>(info);
  texture->asset_id = (uint32_t)_state.texture_assets.size();
  return _state.texture_assets.emplace(path, texture).first->second;
}

Shared<Mesh> AssetManager::load_mesh_asset(const std::string& path, uint32_t loadingFlags) {
  OX_SCOPED_ZONE;
  Shared<Mesh> asset = create_shared<Mesh>(path);
  asset->asset_id = (uint32_t)_state.mesh_assets.size();
  return _state.mesh_assets.emplace(path, asset).first->second;
}

Shared<AudioSource> AssetManager::load_audio_asset(const std::string& path) {
  OX_SCOPED_ZONE;
  Shared<AudioSource> source = create_shared<AudioSource>(path);
  return _state.audio_assets.emplace(path, source).first->second;
}

void AssetManager::free_unused_assets() {
  OX_SCOPED_ZONE;
  const auto m_count = std::erase_if(_state.mesh_assets,
                                     [](const std::pair<std::string, Shared<Mesh>>& pair) { return pair.second.use_count() <= 1; });

  if (m_count > 0)
    OX_LOG_INFO("Cleaned up {} mesh assets.", m_count);

  const auto t_count = std::erase_if(_state.texture_assets,
                                     [](const std::pair<std::string, Shared<Texture>>& pair) { return pair.second.use_count() <= 1; });

  if (t_count > 0)
    OX_LOG_INFO("Cleaned up {} texture assets.", t_count);

  const auto ma_count = std::erase_if(_state.material_assets, [](const Shared<Material>& material) { return material.use_count() <= 1; });

  if (ma_count > 0)
    OX_LOG_INFO("Cleaned up {} material assets.", ma_count);
}
} // namespace ox
