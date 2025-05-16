import common;

import scene;

#define RENDER_FLAGS_2D_FLIP_X 1u << 1u

struct PushConstants {
    Material* materials;
    Camera* camera_buffer;
};
[[vk::push_constant]] PushConstants C;

struct VOutput {
  float4 position : SV_Position;
  float3 normal : NORMAL;
  float4 uv_alpha : UV;
  u32 material_index : MAT_INDEX;
  u32 flags : FLAGS;
};

struct VertexInput {
  [[vk::location(0)]] float4 transform0 : TRANSFORM0;
  [[vk::location(1)]] float4 transform1 : TRANSFORM1;
  [[vk::location(2)]] float4 transform2 : TRANSFORM2;
  [[vk::location(3)]] float4 transform3 : TRANSFORM3;
  [[vk::location(4)]] uint material_id16_ypos16 : MAT_INDEX;
  [[vk::location(5)]] uint flags16_distance16 : FLAGS;
};

static float3 positions[6] =
  {float3(-0.5, -0.5, 0), float3(0.5, -0.5, 0), float3(0.5, 0.5, 0), float3(0.5, 0.5, 0), float3(-0.5, 0.5, 0), float3(-0.5, -0.5, 0)};
static float2 uvs[6] = {float2(0.0, 1.0), float2(1.0, 1.0), float2(1.0, 0.0), float2(1.0, 0.0), float2(0.0, 0.0), float2(0.0, 1.0)};

[[shader("vertex")]]
VOutput vs_main(VertexInput input, uint vertex_id : SV_VertexID) {
  VOutput output = (VOutput)0;

  const u32 flags = com::unpack_u32_low(input.flags16_distance16);
  output.flags = flags;

  const u32 material_index = com::unpack_u32_low(input.material_id16_ypos16);
  Material material = C.materials[material_index];

  mat4 transform = mat4(
    input.transform0,
    input.transform1,
    input.transform2,
    input.transform3
  );
  
  float4x4 unpacked_transform = transpose(transform);
  float4 uv_size_offset = float4(material.uv_size, material.uv_offset);

  const uint vertex_index = vertex_id % 6;

  output.uv_alpha.xy = uvs[vertex_index];
  output.uv_alpha.xy = (output.uv_alpha.xy * uv_size_offset.xy) + (uv_size_offset.zw);

  const int flip = flags & RENDER_FLAGS_2D_FLIP_X;
  float4 world_position = float4(float2(positions[vertex_index].xy * float2((float)((bool)flip ? -1 : 1), 1)), 0, 1);
  world_position = mul(unpacked_transform, world_position);

  output.position = mul(C.camera_buffer[0].projection_view, float4(world_position.xyz, 1.0f));
  output.material_index = material_index;

  return output;
}

[[shader("fragment")]]
float4 ps_main(VOutput input) : SV_Target0 {
  const Material material = C.materials[input.material_index];

  float2 uv = input.uv_alpha.xy;
  float4 color = material.sample_albedo_color(uv);

  return color;
}
