add_requires("stb 2024.06.01", { system = false })

add_requires("miniaudio 0.11.22", { system = false })

add_requires("imgui v1.91.8-docking", { configs = {
    wchar32 = true,
}, system = false })

add_requires("imguizmo v1.91.8-docking")

add_requires("glm 1.0.1", { configs = {
    header_only = true,
    cxx_standard = "20",
}, system = false })

add_requires("entt v3.15.0", { system = false })

add_requires("fastgltf v0.8.0", { system = false, debug = is_mode("debug") })

add_requires("meshoptimizer v0.22", { system = false })

add_requires("fmt 11.1.4", { configs = {
    header_only = true
}, system = false })

add_requires("loguru v2.1.0", { configs = {
    fmt = true,
}, system = false })

add_requires("vk-bootstrap v1.4.307", { system = false })

add_requires("vuk 2025.04.22", { configs = {
    debug_allocations = false,
}, debug = is_mode("debug") or is_mode("asan") })

add_requires("shader-slang v2025.6.3", { system = false })

add_requires("libsdl3 3.2.8", { configs = {
    wayland = true,
    x11 = true,
}, system = false })

add_requires("toml++ v3.4.0", { system = false })

add_requires("rapidjson 2025.02.05", { system = false })

add_requires("joltphysics v5.3.0", { configs = {
    debug_renderer = true,
    rtti = true,
    avx = true,
    avx2 = true,
    lzcnt = true,
    sse4_1 = true,
    sse4_2 = true,
    tzcnt = true,
}, system = false })

add_requires("tracy v0.11.1", { configs = {
    on_demand = true,
    callstack = true,
    callstack_inlines = false,
    code_transfer = true,
    exit = true,
    system_tracing = true,
}, system = false })

add_requires("sol2 v3.3.1", { system = false })

add_requires("enkits v1.11", { system = false })

add_requires("unordered_dense v4.5.0", { system = false })

add_requires("plf_colony v7.41", { system = false })

add_requires("dylib v2.2.1", { system = false })

add_requires("ktx-software v4.4.0", { system = false })

add_requires("simdutf v6.2.0", { system = false })
