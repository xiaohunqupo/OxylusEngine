#pragma once

#include "Asset/AssetFile.hpp"
#include "Asset/AudioSource.hpp"
#include "Asset/Material.hpp"
#include "Asset/Mesh.hpp"
#include "Asset/Texture.hpp"
#include "Core/ESystem.hpp"
#include "Core/UUID.hpp"
#include "Memory/SlotMap.hpp"
#include "Scene/Scene.hpp"

namespace ox {
struct Asset {
  UUID uuid = {};
  std::string path = {};
  AssetType type = AssetType::None;
  union {
    MeshID mesh_id = MeshID::Invalid;
    TextureID texture_id;
    MaterialID material_id;
    SceneID scene_id;
    AudioID audio_id;
  };

  // Reference count of loads
  u64 ref_count = 0;

  auto is_loaded() const -> bool { return mesh_id != MeshID::Invalid; }

  auto acquire_ref() -> void { ++std::atomic_ref(ref_count); }

  auto release_ref() -> bool { return --std::atomic_ref(ref_count) == 0; }
};

using AssetRegistry = ankerl::unordered_dense::map<UUID, Asset>;
class AssetManager : public ESystem {
public:
  void init() override;
  void deinit() override;

  auto registry() const -> const AssetRegistry&;

  auto create_asset(AssetType type,
                    const std::string& path = {}) -> UUID;

  auto load_asset(const UUID& uuid) -> bool;
  auto unload_asset(const UUID& uuid) -> void;

  auto load_mesh(const UUID& uuid) -> bool;
  auto unload_mesh(const UUID& uuid) -> void;

  auto load_texture(const UUID& uuid,
                    const TextureLoadInfo& info = {}) -> bool;
  auto unload_texture(const UUID& uuid) -> void;

  auto load_material(const UUID& uuid,
                     const Material& material_info) -> bool;
  auto unload_material(const UUID& uuid) -> void;

  auto load_scene(const UUID& uuid) -> bool;
  auto unload_scene(const UUID& uuid) -> void;

  auto load_audio(const UUID& uuid) -> bool;
  auto unload_audio(const UUID& uuid) -> void;

  auto get_asset(const UUID& uuid) -> Asset*;

  auto get_mesh(const UUID& uuid) -> Mesh*;
  auto get_mesh(MeshID mesh_id) -> Mesh*;

  auto get_texture(const UUID& uuid) -> Texture*;
  auto get_texture(TextureID texture_id) -> Texture*;

  auto get_material(const UUID& uuid) -> Material*;
  auto get_material(MaterialID material_id) -> Material*;

  auto get_scene(const UUID& uuid) -> Scene*;
  auto get_scene(SceneID scene_id) -> Scene*;

  auto get_audio(const UUID& uuid) -> AudioSource*;
  auto get_audio(AudioID audio_id) -> AudioSource*;

private:
  AssetRegistry asset_registry = {};

  std::shared_mutex registry_mutex = {};
  std::shared_mutex textures_mutex = {};

  SlotMap<Mesh, MeshID> mesh_map = {};
  SlotMap<Texture, TextureID> texture_map = {};
  SlotMap<Material, MaterialID> material_map = {};
  SlotMap<std::unique_ptr<Scene>, SceneID> scene_map = {};
  SlotMap<AudioSource, AudioID> audio_map = {};
};
} // namespace ox
