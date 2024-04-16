#ifndef GLOBALS_HLSLI
#define GLOBALS_HLSLI

#include "Common.hlsli"
#include "Materials.hlsli"

// set 1
[[vk::binding(0, 1)]] ConstantBuffer<CameraCB> Camera;
[[vk::binding(1, 1)]] StructuredBuffer<MeshInstancePointer> MeshInstancePointers;

// set 0
[[vk::binding(0, 0)]] ConstantBuffer<SceneData> Scene;

[[vk::binding(1, 0)]] ByteAddressBuffer Buffers[];

[[vk::binding(2, 0)]] Texture2D<float4> SceneTextures[];
[[vk::binding(3, 0)]] Texture2D<uint> SceneUintTextures[];
[[vk::binding(4, 0)]] TextureCube<float4> SceneCubeTextures[];
[[vk::binding(5, 0)]] Texture2DArray<float4> SceneArrayTextures[];
[[vk::binding(6, 0)]] RWTexture2D<float4> SceneRWTextures[];
[[vk::binding(7, 0)]] Texture2D<float4> MaterialTextureMaps[];

// 0 - linear_sampler_clamped
// 1 - linear_sampler_repeated
// 2 - linear_sampler_repeated_anisotropy
// 3 - nearest_sampler_clamped
// 4 - nearest_sampler_repeated
[[vk::binding(8, 0)]] SamplerState Samplers[5];
[[vk::binding(9, 0)]] SamplerComparisonState ComparisonSamplers[5];

#define LINEAR_CLAMPED_SAMPLER Samplers[0]
#define LINEAR_REPEATED_SAMPLER Samplers[1]
#define LINEAR_REPEATED_SAMPLER_ANISOTROPY Samplers[2]
#define NEAREST_CLAMPED_SAMPLER Samplers[3]
#define NEAREST_REPEATED_SAMPLER Samplers[4]

#define CMP_DEPTH_SAMPLER ComparisonSamplers[0]

SceneData get_scene() { return Scene; }

CameraData get_camera(uint camera_index = 0) { return Camera.camera_data[camera_index]; }

inline MeshInstance load_instance(uint instance_index) {
  return Buffers[get_scene().indices_.mesh_instance_buffer_index].Load<MeshInstance>(instance_index * sizeof(MeshInstance));
}

struct VertexInput {
  uint vertex_index : SV_VertexID;
  uint instance_index : SV_InstanceID;
  [[vk::builtin("DrawIndex")]] uint draw_index : DrawIndex;

  MeshInstancePointer get_instance_pointer(const uint instance_offset) { return MeshInstancePointers[instance_offset + instance_index]; }

  MeshInstance get_instance(const uint instance_offset) { return load_instance(get_instance_pointer(instance_offset).get_instance_index()); }

  float3 get_vertex_position(uint64_t ptr) {
    uint64_t addressOffset = ptr + vertex_index * sizeof(Vertex);
    return vk::RawBufferLoad<float4>(addressOffset).xyz;
  }

  float3 get_vertex_normal(uint64_t ptr) {
    uint64_t addressOffset = ptr + vertex_index * sizeof(Vertex) + sizeof(float4);
    return vk::RawBufferLoad<float4>(addressOffset).xyz;
  }

  float2 get_vertex_uv(uint64_t ptr) {
    uint64_t addressOffset = ptr + vertex_index * sizeof(Vertex) + sizeof(float4) * 2;
    return vk::RawBufferLoad<float4>(addressOffset).xy;
  }

  float4 get_vertex_tangent(uint64_t ptr) {
    uint64_t addressOffset = ptr + vertex_index * sizeof(Vertex) + sizeof(float4) * 3;
    return vk::RawBufferLoad<float4>(addressOffset);
  }
};

// scene textures
Texture2D<float4> get_forward_texture() { return SceneTextures[get_scene().indices_.forward_image_index]; }
Texture2D<float4> get_normal_texture() { return SceneTextures[get_scene().indices_.normal_image_index]; }
Texture2D<float4> get_depth_texture() { return SceneTextures[get_scene().indices_.depth_image_index]; }
Texture2D<float4> get_shadow_atlas() { return SceneTextures[get_scene().indices_.shadow_array_index]; }
Texture2D<float4> get_sky_transmittance_lut_texture() { return SceneTextures[get_scene().indices_.sky_transmittance_lut_index]; }
Texture2D<float4> get_sky_multi_scatter_lut_texture() { return SceneTextures[get_scene().indices_.sky_multiscatter_lut_index]; }
Texture2D<float4> get_bloom_texture() { return SceneTextures[get_scene().indices_.bloom_image_index]; }

// scene r/w textures
RWTexture2D<float4> get_sky_transmittance_lutrw_texture() { return SceneRWTextures[get_scene().indices_.sky_transmittance_lut_index]; }
RWTexture2D<float4> get_sky_multi_scatter_lutrw_texture() { return SceneRWTextures[get_scene().indices_.sky_multiscatter_lut_index]; }

// scene cube textures
TextureCube<float4> get_sky_env_map_texture() { return SceneCubeTextures[get_scene().indices_.sky_env_map_index]; }

// scene uint textures
Texture2D<uint> get_gtao_texture() { return SceneUintTextures[get_scene().indices_.gtao_buffer_image_index]; }

// material textures
SamplerState get_material_sampler(Material material) { return Samplers[material.sampler]; }

Texture2D<float4> get_material_albedo_texture(Material material) { return MaterialTextureMaps[material.albedo_map_id]; }
Texture2D<float4> get_material_normal_texture(Material material) { return MaterialTextureMaps[material.normal_map_id]; }
Texture2D<float4> get_material_physical_texture(Material material) { return MaterialTextureMaps[material.physical_map_id]; }
Texture2D<float4> get_material_ao_texture(Material material) { return MaterialTextureMaps[material.ao_map_id]; }
Texture2D<float4> get_material_emissive_texture(Material material) { return MaterialTextureMaps[material.emissive_map_id]; }

ShaderEntity get_entity(uint index) { return Buffers[get_scene().indices_.entites_buffer_index].Load<ShaderEntity>(index * sizeof(ShaderEntity)); }

Material get_material(int material_index) {
  return Buffers[get_scene().indices_.materials_buffer_index].Load<Material>(material_index * sizeof(Material));
}
Light get_light(int lightIndex) { return Buffers[get_scene().indices_.lights_buffer_index].Load<Light>(lightIndex * sizeof(Light)); }
#endif // GLOBALS_HLSLI
