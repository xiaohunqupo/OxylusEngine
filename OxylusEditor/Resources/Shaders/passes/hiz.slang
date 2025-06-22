module hiz;

import common;
import gpu;

// Do not remove this comment:
// Taken from: https://github.dev/SparkyPotato/radiance/blob/main/shaders/passes/mesh/hzb.slang,
// which is based on https://github.com/Themaister/Granite/blob/master/assets/shaders/post/hiz.comp,
// which is HiZ modification of AMD's Single Pass Downsampler.

struct ShaderParameters {
    globallycoherent RWStructuredBuffer<u32> spd_global_atomic;
    Sampler sampler;
    Image2D<f32> src_image;
    StorageImage2D<f32> dst_mip_0;
    StorageImage2D<f32> dst_mip_1;
    StorageImage2D<f32> dst_mip_2;
    StorageImage2D<f32> dst_mip_3;
    StorageImage2D<f32> dst_mip_4;
    StorageImage2D<f32> dst_mip_5;
    globallycoherent StorageImage2D<f32> dst_mip_6;
    StorageImage2D<f32> dst_mip_7;
    StorageImage2D<f32> dst_mip_8;
    StorageImage2D<f32> dst_mip_9;
    StorageImage2D<f32> dst_mip_10;
    StorageImage2D<f32> dst_mip_11;
    StorageImage2D<f32> dst_mip_12;
};

func get_mip_image(in ShaderParameters params, u32 mip) -> StorageImage2D<f32> {
    switch(mip) {
        case 0: return params.dst_mip_0;
        case 1: return params.dst_mip_1;
        case 2: return params.dst_mip_2;
        case 3: return params.dst_mip_3;
        case 4: return params.dst_mip_4;
        case 5: return params.dst_mip_5;
        case 6: return params.dst_mip_6;
        case 7: return params.dst_mip_7;
        case 8: return params.dst_mip_8;
        case 9: return params.dst_mip_9;
        case 10: return params.dst_mip_10;
        case 11: return params.dst_mip_11;
        default: return params.dst_mip_12;
    }
}

func unswizzle_16x16(u32 i) -> u32x2 {
    let x0 = bitfieldExtract(i, 0, 1);
    let y01 = bitfieldExtract(i, 1, 2);
    let x12 = bitfieldExtract(i, 3, 2);
    let y23 = bitfieldExtract(i, 5, 2);
    let x3 = bitfieldExtract(i, 7, 1);
    return u32x2(bitfieldInsert(bitfieldInsert(x0, x12, 1, 2), x3, 3, 1), bitfieldInsert(y01, y23, 2, 2));
}

func transform_z(in f32x2x2 transform_z, f32x4 zs) -> f32x4 {
    let z0 = mul(transform_z, float2(zs.x, 1.0));
    let z1 = mul(transform_z, float2(zs.y, 1.0));
    let z2 = mul(transform_z, float2(zs.z, 1.0));
    let z3 = mul(transform_z, float2(zs.w, 1.0));
    return f32x4(z0.x, z1.x, z2.x, z3.x) / f32x4(z0.y, z1.y, z2.y, z3.y);
}

func reduce(f32 x, f32 y) -> f32 {
    return min(x, y);
}

func reduce(f32x4 v) -> f32 {
	return reduce(reduce(v.x, v.y), reduce(v.z, v.w));
}

func store(in ShaderParameters params, u32x2 texel, u32 mip, f32 v) -> void {
    get_mip_image(params, mip).store(texel, v);
}

func store_2x2(in ShaderParameters params, u32x2 p, u32 mip, f32x4 v) -> void {
    store(params, p + u32x2(0, 0), mip, v.x);
    store(params, p + u32x2(1, 0), mip, v.y);
    store(params, p + u32x2(0, 1), mip, v.z);
    store(params, p + u32x2(1, 1), mip, v.w);
}

func load(in ShaderParameters params, u32x2 texel) -> f32 {
    u32x2 extent;
    i32 _;
    params.src_image.GetDimensions(0, extent.x, extent.y, _);
    let inv_extent = 1.0 / f32x2(extent);
    f32x2 uv = f32x2(texel) * inv_extent + inv_extent;
    return params.src_image.sample_mip(params.sampler, uv, 0);
}

func load_2x2(in ShaderParameters params, u32x2 p) -> f32x4 {
    let x = load(params, p + u32x2(0, 0));
    let y = load(params, p + u32x2(1, 0));
    let z = load(params, p + u32x2(0, 1));
    let w = load(params, p + u32x2(1, 1));
    return f32x4(x, y, z, w);
}

func load_4x4(in ShaderParameters params, u32x2 p) -> f32x4x4 {
    let x = load_2x2(params, p + u32x2(0, 0));
    let y = load_2x2(params, p + u32x2(2, 0));
    let z = load_2x2(params, p + u32x2(0, 2));
    let w = load_2x2(params, p + u32x2(2, 2));
    return f32x4x4(x, y, z, w);
}

func load_mid_2x2(in ShaderParameters params, u32x2 p) -> f32x4 {
    let x = params.dst_mip_6.load(p + i32x2(0, 0));
    let y = params.dst_mip_6.load(p + i32x2(1, 0));
    let z = params.dst_mip_6.load(p + i32x2(0, 1));
    let w = params.dst_mip_6.load(p + i32x2(1, 1));
    return f32x4(x, y, z, w);
}

func load_mid_4x4(in ShaderParameters params, u32x2 p) -> f32x4x4 {
    let x = load_mid_2x2(params, p + u32x2(0, 0));
    let y = load_mid_2x2(params, p + u32x2(2, 0));
    let z = load_mid_2x2(params, p + u32x2(0, 2));
    let w = load_mid_2x2(params, p + u32x2(2, 2));
    return f32x4x4(x, y, z, w);
}

func reduce_mip(in ShaderParameters params, f32x4x4 v, u32x2 p, u32 mip) -> f32 {
    let d0 = reduce(v[0]);
    let d1 = reduce(v[1]);
    let d2 = reduce(v[2]);
    let d3 = reduce(v[3]);
    let ret = f32x4(d0, d1, d2, d3);
    store_2x2(params, p, mip, ret);
    return reduce(ret);
}

func reduce_mip_simd(in ShaderParameters params, u32 mip_count, u32x2 p, u32 local_id, u32 mip, f32 d) -> f32 {
    var horiz = QuadReadAcrossX(d);
    var vert = QuadReadAcrossY(d);
    var diag = QuadReadAcrossDiagonal(d);
    store(params, p, mip, d);

    if (mip_count > mip + 1) {
        p >>= 1;
        d = reduce(f32x4(d, horiz, vert, diag));
        horiz = com::wave_shuffle_xor(d, 0b1000);
        vert = com::wave_shuffle_xor(d, 0b0100);
        diag = com::wave_shuffle_xor(d, 0b1100);
        if ((local_id & 3) == 0) {
            store(params, p, mip + 1, d);
        }
    }

    return reduce(f32x4(d, horiz, vert, diag));
}

groupshared f32 shared_buffer[256 / 16];
groupshared bool is_last;

[[shader("compute")]]
[[numthreads(256, 1, 1)]]
func cs_main(
    u32x2 group_id : SV_GroupID,
    uniform ParameterBlock<ShaderParameters> params,
    uniform u32 mip_count,
    uniform u32 work_group_count,
    uniform f32x2x2 mat_1
) -> void {
    let local_id = com::subgroup_id() * WaveGetLaneCount() + WaveGetLaneIndex();
    let p = unswizzle_16x16(local_id);
    var base_coord = group_id * 64 + p * 4;

    // MIP 0
    var m = load_4x4(params, base_coord);
    let texel_00 = transform_z(mat_1, m[0]);
    let texel_10 = transform_z(mat_1, m[1]);
    let texel_01 = transform_z(mat_1, m[2]);
    let texel_11 = transform_z(mat_1, m[3]);
    m = f32x4x4(texel_00, texel_10, texel_01, texel_11);

    store_2x2(params, base_coord + int2(0, 0), 0, texel_00);
    store_2x2(params, base_coord + int2(2, 0), 0, texel_10);
    store_2x2(params, base_coord + int2(0, 2), 0, texel_01);
    store_2x2(params, base_coord + int2(2, 2), 0, texel_11);
    if (mip_count <= 1) {
        return;
    }

    // MIP 1
    var d = reduce_mip(params, m, base_coord >> 1, 1);
    if (mip_count <= 2) {
        return;
    }

    // MIP 2, 3
    d = reduce_mip_simd(params, mip_count, base_coord >> 2, local_id, 2, d);
    if (mip_count <= 4) {
        return;
    }

    // MIP 4, 5, 6
    if ((local_id & 15) == 0) {
        shared_buffer[local_id >> 4] = d;
    }

    com::control_barrier(com::memory_order_acq_rel);

    if (local_id < 16) {
        d = reduce_mip_simd(params, mip_count, group_id * 4 + p, local_id, 4, shared_buffer[local_id]);
    }

    if (local_id == 0) {
        store(params, group_id, 6, d);
    }

    if (mip_count <= 6) {
        return;
    }

    com::control_barrier(com::memory_order_acq_rel,
        com::MemoryScope::Workgroup, com::MemoryScope::Queue,
        com::MemoryLocation::Image | com::MemoryLocation::Workgroup);
    
    if (local_id == 0) {
        is_last = com::atomic_add(params.spd_global_atomic[0], 1, com::memory_order_acq_rel) == work_group_count - 1;
    }

    com::control_barrier(com::memory_order_acq_rel);
    if (!is_last) {
        return;
    }

    // MIP 7
    base_coord = p * 4;
    d = reduce_mip(params, load_mid_4x4(params, base_coord), base_coord >> 1, 7);
    if (mip_count <= 8) {
        return;
    }

    // MIP 8, 9
    d = reduce_mip_simd(params, mip_count, p, local_id, 8, d);
    if (mip_count <= 10) {
        return;
    }

    // MIP 10, 11, 12
    if ((local_id & 15) == 0) {
        shared_buffer[local_id >> 4] = d;
    }

    com::control_barrier(com::memory_order_acq_rel);

    if (local_id < 16) {
        d = reduce_mip_simd(params, mip_count, p, local_id, 10, shared_buffer[local_id]);
    }

    if (mip_count <= 12) {
        return;
    }

    if (local_id == 0) {
        store(params, u32x2(0, 0), 12, d);
    }
}
