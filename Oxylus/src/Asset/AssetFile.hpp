#pragma once

namespace ox {
enum class AssetType : uint32 {
  None = 0,
  Shader,
  Model,
  Texture,
  Material,
  Font,
  Scene,
  Audio,
};

// List of file extensions supported by Engine.
enum class AssetFileType : uint32 {
  None = 0,
  Binary,
  Meta,
  GLB,
  GLTF,
  PNG,
  JPEG,
  JSON,
  KTX2,
};

enum class AssetFileFlags : uint64 {
  None = 0,
};
consteval void enable_bitmask(AssetFileFlags);

struct TextureAssetFileHeader {
  vuk::Extent3D extent = {};
  vuk::Format format = vuk::Format::eUndefined;
};

struct AssetFileHeader {
  char8 magic[2] = {'O', 'X'};
  uint16 version = 1;
  AssetFileFlags flags = AssetFileFlags::None;
  AssetType type = AssetType::None;
  union {
    TextureAssetFileHeader texture_header = {};
  };
};
} // namespace ox
