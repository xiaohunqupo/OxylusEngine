cmake_minimum_required(VERSION 3.25.0 FATAL_ERROR)

project(Oxylus CXX C)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_VS_PLATFORM_NAME "x64")

# Global configuration types
set(CMAKE_CONFIGURATION_TYPES
    "Debug"
    "Release"
    "Distribution"
    CACHE STRING "" FORCE
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ASAN
if (ENABLE_ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

# fix for dxc
if (UNIX)
    add_compile_options(-fms-extensions)
endif(UNIX)

# compile commands
if (NOT MSVC)
	set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif ()

# GIT_SUBMODULE "" policy
cmake_policy(SET CMP0097 NEW)

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "")

# Sub-projects
add_subdirectory(Oxylus)
add_subdirectory(OxylusEditor)

