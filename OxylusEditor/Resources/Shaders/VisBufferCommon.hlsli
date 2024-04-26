#include "Common.hlsli"

#define MAX_INDICES 64
#define MAX_PRIMITIVES 64
#define MESHLET_ID_BITS 24u
#define MESHLET_MATERIAL_ID_BITS 23u
#define MESHLET_PRIMITIVE_BITS 8u
#define MESHLET_ID_MASK ((1u << MESHLET_ID_BITS) - 1u)
#define MESHLET_MATERIAL_ID_MASK ((1u << MESHLET_MATERIAL_ID_BITS) - 1u)
#define MESHLET_PRIMITIVE_MASK ((1u << MESHLET_PRIMITIVE_BITS) - 1u)

struct DispatchParams {
  uint32 groupCountX;
  uint32 groupCountY;
  uint32 groupCountZ;
};

[[vk::binding(0, 2)]] ByteAddressBuffer buffers[];
[[vk::binding(1, 2)]] RWByteAddressBuffer buffers_rw[];

#define MESHLET_DATA_BUFFERS_INDEX 0
#define VISIBLE_MESHLETS_BUFFER_INDEX 1
#define CULL_TRIANGLES_DISPATCH_PARAMS_BUFFERS_INDEX 2
#define DRAW_ELEMENTS_INDIRECT_COMMAND_INDEX 3
#define INDEX_BUFFER_INDEX 4
#define VERTEX_BUFFER_INDEX 5
#define PRIMITIVES_BUFFER_INDEX 6
#define MESH_INSTANCES_BUFFER_INDEX 7
#define INSTANCED_INDEX_BUFFER_INDEX 8

Meshlet get_meshlet(uint32 index) { return buffers[MESHLET_DATA_BUFFERS_INDEX].Load<Meshlet>(index * sizeof(Meshlet)); }
Vertex get_vertex(uint32 index) { return buffers[VERTEX_BUFFER_INDEX].Load<Vertex>(index * sizeof(Vertex)); }
uint32 get_primitive(uint32 index) { return buffers[PRIMITIVES_BUFFER_INDEX].Load<uint32>(index * sizeof(uint32)); }
MeshInstance get_instance(uint32 index) { return buffers[MESH_INSTANCES_BUFFER_INDEX].Load<MeshInstance>(index * sizeof(MeshInstance)); }
uint32 get_index(uint32 index) { return buffers_rw[INDEX_BUFFER_INDEX].Load<uint32>(index * sizeof(uint32)); }

uint32 get_draw_elements_indirect_command_idxcount() { return buffers_rw[DRAW_ELEMENTS_INDIRECT_COMMAND_INDEX].Load<uint32>(sizeof(uint32)); }
void set_draw_elements_indirect_command_idxcount(uint32 index) {
  return buffers_rw[DRAW_ELEMENTS_INDIRECT_COMMAND_INDEX].Store<uint32>(sizeof(uint32), index);
}
uint32 get_dispatch_paramsx() { return buffers_rw[CULL_TRIANGLES_DISPATCH_PARAMS_BUFFERS_INDEX].Load<uint32>(sizeof(uint32)); }
void set_dispatch_paramsx(uint32 value) { return buffers_rw[CULL_TRIANGLES_DISPATCH_PARAMS_BUFFERS_INDEX].Store<uint32>(sizeof(uint32), value); }
void set_index(uint32 index, uint32 value) { buffers_rw[INSTANCED_INDEX_BUFFER_INDEX].Store<uint32>(index * sizeof(uint32), value); }

uint32 get_visible_meshlet(uint32 index) { return buffers_rw[VISIBLE_MESHLETS_BUFFER_INDEX].Load<uint32>(index * sizeof(uint32)); }
void set_visible_meshlet(uint32 index, uint32 value) { buffers_rw[VISIBLE_MESHLETS_BUFFER_INDEX].Store<uint32>(index * sizeof(uint32), value); }

bool RectIntersectRect(float2 bottomLeft0, float2 topRight0, float2 bottomLeft1, float2 topRight1) {
  return !(any(topRight0 < bottomLeft1) || any(bottomLeft0 > topRight1));
}
