target("OxylusTests")
    set_kind("binary")
    set_default(false)
    set_languages("cxx23")

    add_deps("Oxylus")

    add_files("**.cpp")
    add_forceincludes("Tracy.hpp")

    on_config(function (target)
        if (target:has_tool("cxx", "msvc", "cl")) then
            target:add("defines", "OX_COMPILER_MSVC=1")
        elseif(target:has_tool("cxx", "clang", "clangxx")) then
            target:add("defines", "OX_COMPILER_CLANG=1")
        elseif target:has_tool("cxx", "gcc", "gxx") then
            target:add("defines", "OX_COMPILER_GCC=1")
        end
    end)

    add_packages("gtest")

    if is_plat("windows") then
        add_defines("_UNICODE", "UNICODE", "WIN32_LEAN_AND_MEAN", "VC_EXTRALEAN", "NOMINMAX", "_WIN32", "_CRT_SECURE_NO_WARNINGS")
    end

    if is_mode("debug") then
        add_defines("OX_DEBUG", "_DEBUG")
    elseif is_mode("release") then
        add_defines("OX_RELEASE", "NDEBUG")
    elseif is_mode("dist") then
        add_defines("OX_DISTRIBUTION", "NDEBUG")
    end

    add_tests("jobmanager")

target_end()
