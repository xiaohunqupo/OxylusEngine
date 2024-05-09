#include "Globals.hlsli"
#include "VisBufferCommon.hlsli"

#define ENABLE_DEBUG_DRAWING

#ifdef ENABLE_DEBUG_DRAWING
void debug_draw_meshlet_aabb(const uint meshletId) {
  const Meshlet meshlet = get_meshlet(meshletId);
  const uint instance_id = meshlet.instanceId;
  const float4x4 transform = get_instance(instance_id).transform;
  const float3 aabb_min = meshlet.aabbMin.unpack();
  const float3 aabb_max = meshlet.aabbMax.unpack();
  const float3 aabb_size = aabb_max - aabb_min;
  const float3 aabb_corners[] = {aabb_min,
                                 aabb_min + float3(aabb_size.x, 0.0, 0.0),
                                 aabb_min + float3(0.0, aabb_size.y, 0.0),
                                 aabb_min + float3(0.0, 0.0, aabb_size.z),
                                 aabb_min + float3(aabb_size.xy, 0.0),
                                 aabb_min + float3(0.0, aabb_size.yz),
                                 aabb_min + float3(aabb_size.x, 0.0, aabb_size.z),
                                 aabb_min + aabb_size};

  float3 world_aabb_min = float3(1e20, 1e20, 1e20);
  float3 world_aabb_max = float3(-1e20, -1e20, -1e20);
  for (uint i = 0; i < 8; ++i) {
    float3 world = float3(mul(transform, float4(aabb_corners[i], 1.0)).xyz);
    world_aabb_min = min(world_aabb_min, world);
    world_aabb_max = max(world_aabb_max, world);
  }

  const float3 aabb_center = (world_aabb_min + world_aabb_max) / 2.0;
  PackedFloat3 center_packed = (PackedFloat3)0;
  center_packed.pack(aabb_center);

  const float3 extent = (world_aabb_max - world_aabb_min);
  PackedFloat3 extent_packed = (PackedFloat3)0;
  extent_packed.pack(extent);

  const float GOLDEN_CONJ = 0.6180339887498948482045868343656;
  const float4 color = float4(2.0 * hsv_to_rgb(float3(float(meshletId) * GOLDEN_CONJ, 0.875, 0.85)), 1.0);
  PackedFloat4 color_packed = (PackedFloat4)0;
  color_packed.pack(color);

  DebugAabb aabb;
  aabb.extent = extent_packed;
  aabb.center = center_packed;
  aabb.color = color_packed;

  try_push_debug_aabb(aabb);
}
#endif // ENABLE_DEBUG_DRAWING

#if 0
bool is_on_plane(in float3 center, in float3 extent, const float4 plane) {
  // projection interval radius of b onto L(t) = b.c + t * p.n
  const float r = extent.x * abs(plane.xyz.x) + extent.y * abs(plane.xyz.y) + extent.z * abs(plane.xyz.z);

  return -r <= dot(plane.xyz, center) - plane.w;
}
#endif

bool is_aabb_inside_plane(in float3 center, in float3 extent, in float4 plane) {
  const float3 normal = plane.xyz;
  const float radius = dot(extent, abs(normal));
  return (dot(normal, center) - plane.w) >= -radius;
}

struct GetMeshletUvBoundsParams {
  uint meshlet_id;
  float4x4 view_proj;
  bool clamp_ndc;
};

void get_meshlet_uv_bounds(GetMeshletUvBoundsParams params, out float2 minXY, out float2 maxXY, out float nearestZ, out int intersects_near_plane) {
  const Meshlet meshlet = get_meshlet(params.meshlet_id);
  const uint instanceId = meshlet.instanceId;
  const float4x4 transform = get_instance(instanceId).transform;
  const float3 aabb_min = meshlet.aabbMin.unpack();
  const float3 aabb_max = meshlet.aabbMax.unpack();
  const float3 aabb_size = aabb_max - aabb_min;
  const float3 aabb_corners[] = {aabb_min,
                                 aabb_min + float3(aabb_size.x, 0.0, 0.0),
                                 aabb_min + float3(0.0, aabb_size.y, 0.0),
                                 aabb_min + float3(0.0, 0.0, aabb_size.z),
                                 aabb_min + float3(aabb_size.xy, 0.0),
                                 aabb_min + float3(0.0, aabb_size.yz),
                                 aabb_min + float3(aabb_size.x, 0.0, aabb_size.z),
                                 aabb_min + aabb_size};

  // The nearest projected depth of the object's AABB
  nearestZ = 0;

  // Min and max projected coordinates of the object's AABB in UV space
  minXY = float2(1e20, 1e20);
  maxXY = float2(-1e20, -1e20);
  const float4x4 mvp = mul(params.view_proj, transform);
  for (uint i = 0; i < 8; ++i) {
    float4 clip = mul(mvp, float4(aabb_corners[i], 1.0));

    // AABBs that go behind the camera at all are considered visible
    if (clip.w <= 0) {
      intersects_near_plane = true;
      return;
    }

    clip.z = max(clip.z, 0.0);
    clip /= clip.w;
    if (params.clamp_ndc) {
      clip.xy = clamp(clip.xy, -1.0, 1.0);
    }
    clip.xy = clip.xy * 0.5 + 0.5;
    minXY = min(minXY, clip.xy);
    maxXY = max(maxXY, clip.xy);
    nearestZ = clamp(max(nearestZ, clip.z), 0.0, 1.0);
  }

  intersects_near_plane = false;
}

bool cull_quad_hiz(float2 minXY, float2 maxXY, float nearestZ) {
  const float4 boxUvs = float4(minXY, maxXY);
  Texture2D<float> hiz_texture = get_hiz_texture();
  float hizwidth, hizheight;
  hiz_texture.GetDimensions(hizwidth, hizheight);
  const float2 hzbSize = float2(hizwidth, hizheight);
  const float width = (boxUvs.z - boxUvs.x) * hzbSize.x;
  const float height = (boxUvs.w - boxUvs.y) * hzbSize.y;

  // Select next level so the box is always in [0.5, 1.0) of a texel of the current level.
  // If the box is larger than a single texel of the current level, then it could touch nine
  // texels rather than four! So we need to round up to the next level.
  const float level = ceil(log2(max(width, height)));
  const float depth[4] = {hiz_texture.SampleLevel(HIZ_SAMPLER, boxUvs.xy, level).x,
                          hiz_texture.SampleLevel(HIZ_SAMPLER, boxUvs.zy, level).x,
                          hiz_texture.SampleLevel(HIZ_SAMPLER, boxUvs.xw, level).x,
                          hiz_texture.SampleLevel(HIZ_SAMPLER, boxUvs.zw, level).x};
  const float farHZB = min(min(min(depth[0], depth[1]), depth[2]), depth[3]);

  // Object is occluded if its nearest depth is farther away from the camera than the farthest sampled depth
  if (nearestZ < farHZB) {
    return false;
  }

  return true;
}

bool cull_meshlet_frustum(const uint meshletId) {
  const Meshlet meshlet = get_meshlet(meshletId);
  const uint instanceId = meshlet.instanceId;
  const float4x4 transform = get_instance(instanceId).transform; // TODO: instanceId
  const float3 aabbMin = meshlet.aabbMin.unpack();
  const float3 aabbMax = meshlet.aabbMax.unpack();
  const float3 aabbCenter = (aabbMin + aabbMax) * 0.5f;
  const float3 aabbExtent = aabbMax - aabbCenter;
  const float3 worldAabbCenter = float3(mul(transform, float4(aabbCenter, 1.0)).xyz);
  const float3 right = transform[0].xyz * aabbExtent.x;
  const float3 up = transform[1].xyz * aabbExtent.y;
  const float3 forward = -transform[2].xyz * aabbExtent.z;

  const float3 worldExtent = float3(abs(dot(float3(1.0, 0.0, 0.0), right)) + abs(dot(float3(1.0, 0.0, 0.0), up)) +
                                      abs(dot(float3(1.0, 0.0, 0.0), forward)),

                                    abs(dot(float3(0.0, 1.0, 0.0), right)) + abs(dot(float3(0.0, 1.0, 0.0), up)) +
                                      abs(dot(float3(0.0, 1.0, 0.0), forward)),

                                    abs(dot(float3(0.0, 0.0, 1.0), right)) + abs(dot(float3(0.0, 0.0, 1.0), up)) +
                                      abs(dot(float3(0.0, 0.0, 1.0), forward)));

  for (uint i = 0; i < 6; ++i) {
    if (!is_aabb_inside_plane(worldAabbCenter, worldExtent, get_camera(0).frustum_planes[i])) {
      return false;
    }
  }

  return true;
}

[numthreads(128, 1, 1)] void main(uint3 threadID
                                  : SV_DispatchThreadID) {
  const uint meshletId = threadID.x;

  if (meshletId >= get_scene().meshlet_count) {
    return;
  }

  if (cull_meshlet_frustum(meshletId)) {
    GetMeshletUvBoundsParams params;
    params.meshlet_id = meshletId;
    params.view_proj = get_camera(0).projection_view;
    params.clamp_ndc = true;

    float2 minXY;
    float2 maxXY;
    float nearestZ;
    bool intersects_near_plane;
    get_meshlet_uv_bounds(params, minXY, maxXY, nearestZ, intersects_near_plane);
    bool is_visible = intersects_near_plane;

    const bool CULL_MESHLET_HIZ = false;
    if (!is_visible) {
      if (!CULL_MESHLET_HIZ) {
        is_visible = true;
      } else {
        // Hack to get around apparent precision issue for tiny meshlets
        is_visible = cull_quad_hiz(minXY, maxXY, nearestZ + 0.0001);
      }
    }

    if (is_visible) {
      uint idx = 0;
      buffers_rw[CULL_TRIANGLES_DISPATCH_PARAMS_BUFFERS_INDEX].InterlockedAdd(0, 1, idx);
      buffers_rw[VISIBLE_MESHLETS_BUFFER_INDEX].Store<uint32>(idx * sizeof(uint32), meshletId);

#ifdef ENABLE_DEBUG_DRAWING
      debug_draw_meshlet_aabb(meshletId);
      // DebugRect rect;
      // rect.minOffset = Vec2ToPacked(minXY);
      // rect.maxOffset = Vec2ToPacked(maxXY);
      // const float GOLDEN_CONJ = 0.6180339887498948482045868343656;
      // float4 color = float4(2.0 * hsv_to_rgb(float3(float(meshletId) * GOLDEN_CONJ, 0.875, 0.85)), 1.0);
      // rect.color = Vec4ToPacked(color);
      // rect.depth = nearestZ;
      // TryPushDebugRect(rect);
#endif // ENABLE_DEBUG_DRAWING
    }
  }
}
