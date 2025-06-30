module cull;

import common;
import scene;

public func test_aabb_vs_plane(f32x3 center, f32x3 extent, f32x4 plane) -> bool {
    const f32x3 normal = plane.xyz;
    const f32 radius = dot(extent, abs(normal));
    return (dot(normal, center) - plane.w) >= -radius;
}

public func normalize_plane(f32x4 p) -> f32x4 {
    return p / length(p.xyz);
}

vector<T, N> min8<T : __BuiltinFloatingPointType, let N : int>(
    vector<T, N> p0, vector<T, N> p1, vector<T, N> p2,
    vector<T, N> p3, vector<T, N> p4, vector<T, N> p5,
    vector<T, N> p6, vector<T, N> p7) {
    return min(p0, min(p1, min(p2, min(p3, min(p4, min(p5, min(p6, p7)))))));
}

vector<T, N> max8<T : __BuiltinFloatingPointType, let N : int>(
    vector<T, N> p0, vector<T, N> p1, vector<T, N> p2,
    vector<T, N> p3, vector<T, N> p4, vector<T, N> p5,
    vector<T, N> p6, vector<T, N> p7) {
    return max(p0, max(p1, max(p2, max(p3, max(p4, max(p5, max(p6, p7)))))));
}

// https://zeux.io/2023/01/12/approximate-projected-bounds
public func project_aabb(f32x4x4 mvp, f32 near, f32x3 aabb_center, f32x3 aabb_extent) -> Optional<MeshletBounds> {
    let SX = mul(mvp, f32x4(aabb_extent.x, 0.0, 0.0, 0.0));
    let SY = mul(mvp, f32x4(0.0, aabb_extent.y, 0.0, 0.0));
    let SZ = mul(mvp, f32x4(0.0, 0.0, aabb_extent.z, 0.0));

    let aabb_half_extent = aabb_extent * 0.5;
    let P0 = mul(mvp, f32x4(aabb_center - aabb_half_extent, 1.0));
    let P1 = P0 + SZ;
    let P2 = P0 + SY;
    let P3 = P2 + SZ;
    let P4 = P0 + SX;
    let P5 = P4 + SZ;
    let P6 = P4 + SY;
    let P7 = P6 + SZ;

    let depth = min8(P0, P1, P2, P3, P4, P5, P6, P7).w;
    if (depth < near)
        return none;

    let DP0 = P0.xyz / P0.w;
    let DP1 = P1.xyz / P1.w;
    let DP2 = P2.xyz / P2.w;
    let DP3 = P3.xyz / P3.w;
    let DP4 = P4.xyz / P4.w;
    let DP5 = P5.xyz / P5.w;
    let DP6 = P6.xyz / P6.w;
    let DP7 = P7.xyz / P7.w;
    let min = min8(DP0, DP1, DP2, DP3, DP4, DP5, DP6, DP7);
    let max = max8(DP0, DP1, DP2, DP3, DP4, DP5, DP6, DP7);
    var vaabb = f32x4(min.xy, max.xy);
    vaabb = vaabb.xwzy * f32x4(0.5f, -0.5f, 0.5f, -0.5f) + 0.5f;
    return MeshletBounds(f32x3(vaabb.xy, min.z), f32x3(vaabb.zw, max.z));
}

