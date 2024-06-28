#include "Globals.hlsli"

struct VOutput {
  float4 position : SV_Position;
  float3 normal : NORMAL;
  float4 uv_alpha : UV;
  uint32 material_index : MAT_INDEX;
};

struct VertexInput {
  float4 position : POSITION;
  float2 size : SIZE;
  uint32 material_index : MAT_INDEX;
  uint32 flags : FLAGS;
};

static float3 positions[6] =
  {float3(-0.5, -0.5, 0), float3(0.5, -0.5, 0), float3(0.5, 0.5, 0), float3(0.5, 0.5, 0), float3(-0.5, 0.5, 0), float3(-0.5, -0.5, 0)};
static float2 uvs[6] = {float2(0.0, 1.0), float2(1.0, 1.0), float2(1.0, 0.0), float2(1.0, 0.0), float2(0.0, 0.0), float2(0.0, 1.0)};

VOutput VSmain(VertexInput input, uint vertex_id : SV_VertexID) {
  VOutput output = (VOutput)0;

  SpriteMaterial material = get_sprite_material(input.material_index);

  float3 position = input.position;
  float4 uv_size_offset = float4(material.get_uv_size(), material.get_uv_offset());
  float2 size = input.size;

  const uint vertex_index = vertex_id % 6;

  output.uv_alpha.xy = uvs[vertex_index];
  output.uv_alpha.xy = output.uv_alpha.xy * uv_size_offset.xy + uv_size_offset.zw;

  const float facing = input.flags; // position.w;

  const float screen_space = 0;
  float4 world_position = float4(float2(positions[vertex_index].xy * size.xy), 0, 1);
  world_position.xyz += position.xyz;

  output.position = mul(get_camera(0).projection_view, float4(world_position.xyz, 1.0f));
  output.material_index = input.material_index;

  return output;
}

float4 PSmain(VOutput input) : SV_Target0 {
  SpriteMaterial material = get_sprite_material(input.material_index);
  const SamplerState material_sampler = LINEAR_REPEATED_SAMPLER;

  float4 color = material.color.unpack();

  if (material.albedo_map_id != INVALID_ID) {
    color *= get_sprite_material_albedo_texture(material).Sample(material_sampler, input.uv_alpha.xy);
  }

  return color;
}
