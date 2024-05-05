#include "Globals.hlsli"
#include "VisBufferCommon.hlsli"

groupshared uint sh_baseIndex;
groupshared uint sh_primitivesPassed;
groupshared float4x4 sh_mvp;

// Taken from:
// https://github.com/GPUOpen-Effects/GeometryFX/blob/master/amd_geometryfx/src/Shaders/AMD_GeometryFX_Filtering.hlsl
// Parameters: vertices in UV space, viewport extent
bool CullSmallPrimitive(float2 vertices[3], float2 viewportExtent) {
  const uint SUBPIXEL_BITS = 8;
  const uint SUBPIXEL_MASK = 0xFF;
  const uint SUBPIXEL_SAMPLES = 1 << SUBPIXEL_BITS;
  /**
  Computing this in float-point is not precise enough
  We switch to a 23.8 representation here which should match the
  HW subpixel resolution.
  We use a 8-bit wide guard-band to avoid clipping. If
  a triangle is outside the guard-band, it will be ignored.

  That is, the actual viewport supported here is 31 bit, one bit is
  unused, and the guard band is 1 << 23 bit large (8388608 pixels)
  */

  int2 minBB = int2(1 << 30, 1 << 30);
  int2 maxBB = int2(-(1 << 30), -(1 << 30));

  for (uint i = 0; i < 3; ++i) {
    float2 screenSpacePositionFP = vertices[i].xy * viewportExtent;
    // Check if we would overflow after conversion
    if (screenSpacePositionFP.x < -(1 << 23) || screenSpacePositionFP.x > (1 << 23) || screenSpacePositionFP.y < -(1 << 23) ||
        screenSpacePositionFP.y > (1 << 23)) {
      return true;
    }

    int2 screenSpacePosition = int2(screenSpacePositionFP * SUBPIXEL_SAMPLES);
    minBB = min(screenSpacePosition, minBB);
    maxBB = max(screenSpacePosition, maxBB);
  }

  /**
  Test is:

  Is the minimum of the bounding box right or above the sample
  point and is the width less than the pixel width in samples in
  one direction.

  This will also cull very long triangles which fall between
  multiple samples.
  */
  return !((((minBB.x & SUBPIXEL_MASK) > SUBPIXEL_SAMPLES / 2) &&
            ((maxBB.x - ((minBB.x & ~SUBPIXEL_MASK) + SUBPIXEL_SAMPLES / 2)) < (SUBPIXEL_SAMPLES - 1))) ||
           (((minBB.y & SUBPIXEL_MASK) > SUBPIXEL_SAMPLES / 2) &&
            ((maxBB.y - ((minBB.y & ~SUBPIXEL_MASK) + SUBPIXEL_SAMPLES / 2)) < (SUBPIXEL_SAMPLES - 1))));
}

// Returns true if the triangle is visible
// https://www.slideshare.net/gwihlidal/optimizing-the-graphics-pipeline-with-compute-gdc-2016
bool CullTriangle(Meshlet meshlet, uint localId) {
  // Skip if no culling flags are enabled
  const bool culling_disabled = false;
  if (culling_disabled) {
    return true;
  }

  const uint primitiveId = localId * 3;

  // TODO: SoA vertices
  const uint vertexOffset = meshlet.vertexOffset;
  const uint indexOffset = meshlet.indexOffset;
  const uint primitiveOffset = meshlet.primitiveOffset;
  const uint primitive0 = uint(get_primitive(primitiveOffset + primitiveId + 0));
  const uint primitive1 = uint(get_primitive(primitiveOffset + primitiveId + 1));
  const uint primitive2 = uint(get_primitive(primitiveOffset + primitiveId + 2));
  const uint index0 = get_index(indexOffset + primitive0);
  const uint index1 = get_index(indexOffset + primitive1);
  const uint index2 = get_index(indexOffset + primitive2);
  const Vertex vertex0 = get_vertex(vertexOffset + index0);
  const Vertex vertex1 = get_vertex(vertexOffset + index1);
  const Vertex vertex2 = get_vertex(vertexOffset + index2);
  const float3 position0 = vertex0.position;
  const float3 position1 = vertex1.position;
  const float3 position2 = vertex2.position;
  const float4 posClip0 = mul(sh_mvp, float4(position0, 1.0));
  const float4 posClip1 = mul(sh_mvp, float4(position1, 1.0));
  const float4 posClip2 = mul(sh_mvp, float4(position2, 1.0));

  // Backfacing and zero-area culling
  // https://redirect.cs.umbc.edu/~olano/papers/2dh-tri/
  // This is equivalent to the HLSL code that was ported, except the mat3 is transposed.
  // However, the determinant of a matrix and its transpose are the same, so this is fine.
  const bool cull_primitive_backface = false; // TODO:
  if (cull_primitive_backface) {
    const float det = determinant(float3x3(posClip0.xyw, posClip1.xyw, posClip2.xyw));
    if (det <= 0) {
      return false;
    }
  }

  const float3 posNdc0 = posClip0.xyz / posClip0.w;
  const float3 posNdc1 = posClip1.xyz / posClip1.w;
  const float3 posNdc2 = posClip2.xyz / posClip2.w;

  const float2 bboxNdcMin = min(posNdc0.xy, min(posNdc1.xy, posNdc2.xy));
  const float2 bboxNdcMax = max(posNdc0.xy, max(posNdc1.xy, posNdc2.xy));

  const bool allBehind = posNdc0.z < 0 && posNdc1.z < 0 && posNdc2.z < 0;
  if (allBehind) {
    return false;
  }

  const bool anyBehind = posNdc0.z < 0 || posNdc1.z < 0 || posNdc2.z < 0;
  if (anyBehind) {
    return true;
  }

  const bool frustum_culling = false; // TODO: 
  const bool small_primitive_culling = false; // TODO:

  // Frustum culling
  if (frustum_culling) {
    if (!rect_intersect_rect(bboxNdcMin, bboxNdcMax, float2(-1.0, -1.0), float2(1.0, 1.0))) {
      return false;
    }
  }

  // Small primitive culling
  if (small_primitive_culling) {
    const float2 posUv0 = posNdc0.xy * 0.5 + 0.5;
    const float2 posUv1 = posNdc1.xy * 0.5 + 0.5;
    const float2 posUv2 = posNdc2.xy * 0.5 + 0.5;
    float2 arg[3] = {posUv0, posUv1, posUv2};
    if (!CullSmallPrimitive(arg, get_scene().screen_size.xy)) {
      return false;
    }
  }

  return true;
}

[numthreads(MAX_PRIMITIVES, 1, 1)] void main(uint3 groupID
                                             : SV_GroupID, uint3 invocationID
                                             : SV_GroupThreadID) {
  const uint meshletId = get_visible_meshlet(groupID.x);
  const Meshlet meshlet = get_meshlet(meshletId);
  const uint localId = invocationID.x;
  const uint primitiveId = localId * 3;
  const MeshInstance instance = get_instance(meshlet.instanceId);

  if (localId == 0) {
    sh_primitivesPassed = 0;
    sh_mvp = mul(get_camera(0).projection_view, instance.transform);
  }

  GroupMemoryBarrierWithGroupSync();

  bool primitivePassed = false;
  uint activePrimitiveId = 0;
  if (localId < meshlet.primitiveCount) {
    primitivePassed = CullTriangle(meshlet, localId);
    if (primitivePassed) {
      InterlockedAdd(sh_primitivesPassed, 1, activePrimitiveId);
    }
  }

  GroupMemoryBarrierWithGroupSync();

  if (localId == 0) {
    buffers_rw[INDIRECT_COMMAND_BUFFER_INDEX].InterlockedAdd(0, sh_primitivesPassed * 3, sh_baseIndex);
  }

  GroupMemoryBarrierWithGroupSync();

  if (primitivePassed) {
    const uint indexOffset = sh_baseIndex + activePrimitiveId * 3;
    set_index(indexOffset + 0, (meshletId << MESHLET_PRIMITIVE_BITS) | ((primitiveId + 0) & MESHLET_PRIMITIVE_MASK));
    set_index(indexOffset + 1, (meshletId << MESHLET_PRIMITIVE_BITS) | ((primitiveId + 1) & MESHLET_PRIMITIVE_MASK));
    set_index(indexOffset + 2, (meshletId << MESHLET_PRIMITIVE_BITS) | ((primitiveId + 2) & MESHLET_PRIMITIVE_MASK));
  }
}
