#include "Globals.hlsli"
#include "VisBufferCommon.hlsli"

#ifdef ENABLE_DEBUG_DRAWING
void DebugDrawMeshletAabb(in uint meshletId) {
  const Meshlet meshlet = MeshletDataBuffers[meshletDataIndex].meshlets[meshletId];
  const uint instanceId = meshlet.instanceId;
  const float4x4 transform = TransformBuffers[transformsIndex].transforms[instanceId].modelCurrent;
  const float3 aabbMin = meshlet.aabbMin;
  const float3 aabbMax = meshlet.aabbMax;
  const float3 aabbSize = aabbMax - aabbMin;
  const float3 aabbCorners[] = {aabbMin,
                                aabbMin + float3(aabbSize.x, 0.0, 0.0),
                                aabbMin + float3(0.0, aabbSize.y, 0.0),
                                aabbMin + float3(0.0, 0.0, aabbSize.z),
                                aabbMin + float3(aabbSize.xy, 0.0),
                                aabbMin + float3(0.0, aabbSize.yz),
                                aabbMin + float3(aabbSize.x, 0.0, aabbSize.z),
                                aabbMin + aabbSize};

  float3 worldAabbMin = float3(1e20, 1e20, 1e20);
  float3 worldAabbMax = float3(-1e20, -1e20, -1e20);
  for (uint i = 0; i < 8; ++i) {
    float3 world = float3(transform * float4(aabbCorners[i], 1.0));
    worldAabbMin = min(worldAabbMin, world);
    worldAabbMax = max(worldAabbMax, world);
  }

  const float3 aabbCenter = (worldAabbMin + worldAabbMax) / 2.0;
  const float3 extent = (worldAabbMax - worldAabbMin);

  const float GOLDEN_CONJ = 0.6180339887498948482045868343656;
  float4 color = float4(2.0 * hsv_to_rgb(float3(float(meshletId) * GOLDEN_CONJ, 0.875, 0.85)), 1.0);
  TryPushDebugAabb(debugAabbBufferIndex, DebugAabb(Vec3ToPacked(aabbCenter), Vec3ToPacked(extent), Vec4ToPacked(color)));
}
#endif // ENABLE_DEBUG_DRAWING

bool IsAABBInsidePlane(in float3 center, in float3 extent, in float4 plane) {
  const float3 normal = plane.xyz;
  const float radius = dot(extent, abs(normal));
  return (dot(normal, center) - plane.w) >= -radius;
}

struct GetMeshletUvBoundsParams {
  uint meshletId;
  float4x4 viewProj;
  bool clampNdc;
  bool reverseZ;
};

void GetMeshletUvBounds(GetMeshletUvBoundsParams params, out float2 minXY, out float2 maxXY, out float nearestZ, out int intersectsNearPlane) {
  const Meshlet meshlet = get_meshlet(params.meshletId);
  const uint instanceId = meshlet.instanceId;
  const MeshInstance instance = get_instance(instanceId);
  const float4x4 transform = instance.transform;
  const float3 aabbMin = meshlet.aabbMin;
  const float3 aabbMax = meshlet.aabbMax;
  const float3 aabbSize = aabbMax - aabbMin;
  const float3 aabbCorners[] = {aabbMin,
                                aabbMin + float3(aabbSize.x, 0.0, 0.0),
                                aabbMin + float3(0.0, aabbSize.y, 0.0),
                                aabbMin + float3(0.0, 0.0, aabbSize.z),
                                aabbMin + float3(aabbSize.xy, 0.0),
                                aabbMin + float3(0.0, aabbSize.yz),
                                aabbMin + float3(aabbSize.x, 0.0, aabbSize.z),
                                aabbMin + aabbSize};

  // The nearest projected depth of the object's AABB
  if (params.reverseZ) {
    nearestZ = 0;
  } else {
    nearestZ = 1;
  }

  // Min and max projected coordinates of the object's AABB in UV space
  minXY = float2(1e20, 1e20);
  maxXY = float2(-1e20, -1e20);
  const float4x4 mvp = params.viewProj * transform;
  for (uint i = 0; i < 8; ++i) {
    float4 clip = mul(mvp, float4(aabbCorners[i], 1.0));

    // AABBs that go behind the camera at all are considered visible
    if (clip.w <= 0) {
      intersectsNearPlane = true;
      return;
    }

    clip.z = max(clip.z, 0.0);
    clip /= clip.w;
    if (params.clampNdc) {
      clip.xy = clamp(clip.xy, -1.0, 1.0);
    }
    clip.xy = clip.xy * 0.5 + 0.5;
    minXY = min(minXY, clip.xy);
    maxXY = max(maxXY, clip.xy);
    if (params.reverseZ) {
      nearestZ = clamp(max(nearestZ, clip.z), 0.0, 1.0);
    } else {
      nearestZ = clamp(min(nearestZ, clip.z), 0.0, 1.0);
    }
  }

  intersectsNearPlane = false;
}

bool CullQuadHiz(float2 minXY, float2 maxXY, float nearestZ) {
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

bool CullMeshletFrustum(in uint meshletId, CameraData camera) {
  const Meshlet meshlet = get_meshlet(meshletId);
  const uint instanceId = meshlet.instanceId;
  const MeshInstance instance = get_instance(instanceId);
  const float4x4 transform = instance.transform;
  const float3 aabbMin = meshlet.aabbMin;
  const float3 aabbMax = meshlet.aabbMax;
  const float3 aabbCenter = (aabbMin + aabbMax) / 2.0;
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
    if (!IsAABBInsidePlane(worldAabbCenter, worldExtent, camera.frustum_planes[i])) {
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

  if (CullMeshletFrustum(meshletId, get_camera(0))) {
    bool isVisible = false;

    GetMeshletUvBoundsParams params;
    params.meshletId = meshletId;

    params.viewProj = get_camera(0).previous_projection_view;
    params.clampNdc = true;
    params.reverseZ = true;

    float2 minXY;
    float2 maxXY;
    float nearestZ;
    bool intersectsNearPlane;
    GetMeshletUvBounds(params, minXY, maxXY, nearestZ, intersectsNearPlane);
    isVisible = intersectsNearPlane;

    const bool CULL_MESHLET_HIZ = true;
    if (!isVisible) {
      if ((CULL_MESHLET_HIZ) == 0) {
        isVisible = true;
      } else {
        // Hack to get around apparent precision issue for tiny meshlets
        isVisible = CullQuadHiz(minXY, maxXY, nearestZ + 0.0001);
      }
    }

    if (isVisible) {
      uint idx = 0;
      buffers_rw[CULL_TRIANGLES_DISPATCH_PARAMS_BUFFERS_INDEX].InterlockedAdd(0, 1, idx);
      set_visible_meshlet(idx, meshletId);

#ifdef ENABLE_DEBUG_DRAWING
      if (d_currentView.type == VIEW_TYPE_MAIN) {
        DebugDrawMeshletAabb(meshletId);
        // DebugRect rect;
        // rect.minOffset = Vec2ToPacked(minXY);
        // rect.maxOffset = Vec2ToPacked(maxXY);
        // const float GOLDEN_CONJ = 0.6180339887498948482045868343656;
        // float4 color = float4(2.0 * hsv_to_rgb(float3(float(meshletId) * GOLDEN_CONJ, 0.875, 0.85)), 1.0);
        // rect.color = Vec4ToPacked(color);
        // rect.depth = nearestZ;
        // TryPushDebugRect(rect);
      }
#endif // ENABLE_DEBUG_DRAWING
    }
  }
}
