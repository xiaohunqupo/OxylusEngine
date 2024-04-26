#include "Globals.hlsli"
#include "VisBufferCommon.hlsli"

struct VOut {
  float4 position : SV_POSITION;
  uint meshletId : MESHLET_ID;
  uint primitiveId : PRIMITIVE_ID;
  float2 uv : UV0;
  float3 objectSpacePos : OBJECT_SPACE_POS;
};

VOut VSmain(uint vertex_index : SV_VertexID) {
  VOut vout;

  const uint meshletId = (vertex_index >> MESHLET_PRIMITIVE_BITS) & MESHLET_ID_MASK;
  const uint primitiveId = vertex_index & MESHLET_PRIMITIVE_MASK;
  const uint vertexOffset = get_meshlet(meshletId).vertexOffset;
  const uint indexOffset = get_meshlet(meshletId).indexOffset;
  const uint primitiveOffset = get_meshlet(meshletId).primitiveOffset;
  const uint instanceId = get_meshlet(meshletId).instanceId;

  const uint primitive = uint(get_primitive(primitiveOffset + primitiveId));
  const uint index = get_index(indexOffset + primitive);
  const Vertex vertex = get_vertex(vertexOffset + index);
  const float3 position = vertex.position;
  const float2 uv = vertex.uv;
  const float4x4 transform = get_instance(instanceId).transform;

  vout.meshletId = meshletId;
  vout.primitiveId = primitiveId / 3;
  vout.uv = uv;
  vout.objectSpacePos = position;

  vout.position = mul(get_camera(0).projection_view * transform, float4(position, 1.0));

  return vout;
}

uint PSmain(VOut input) : SV_TARGET {
  const Meshlet meshlet = get_meshlet(input.meshletId);
  const Material material = get_material(meshlet.materialId);

  float2 scaledUV = input.uv;
  scaledUV *= material.uv_scale;

  float4 baseColor;
  const SamplerState materialSampler = Samplers[material.sampler];
  if (material.albedo_map_id != INVALID_ID) {
    baseColor = get_material_albedo_texture(material).Sample(materialSampler, scaledUV) * material.color;
  } else {
    baseColor = material.color;
  }

  if (material.alpha_mode == ALPHA_MODE_MASK) {
    clip(baseColor.a - material.alpha_cutoff);
  }

  return (input.meshletId << MESHLET_PRIMITIVE_BITS) | (input.primitiveId & MESHLET_PRIMITIVE_MASK);
}
