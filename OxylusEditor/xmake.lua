target("OxylusEditor")
    set_kind("binary")
    set_languages("cxx23")

    add_includedirs("./src")
    add_files("./src/**.cpp")

    add_deps("Oxylus")

target_end()
