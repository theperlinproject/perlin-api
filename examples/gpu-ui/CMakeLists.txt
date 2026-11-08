cmake_minimum_required(VERSION 3.28)
project(gpu_ui LANGUAGES CXX)

# Without a conduit checkout (tools/pack_shaders.py) and slangc (Vulkan SDK) the plugin still builds, with an
# empty shader pack and a warning. To get real shaders, pass the tools:
# emcmake cmake -S . -B build -DCONDUIT_DIR=/path/to/conduit [-DSLANGC=/path/to/slangc]
include(FetchContent)
if(NOT TARGET perlin::api)
    FetchContent_Declare(perlin_api GIT_REPOSITORY https://github.com/theperlinproject/perlin-api.git GIT_TAG main)
    FetchContent_MakeAvailable(perlin_api)
    include("${perlin_api_SOURCE_DIR}/cmake/PerlinApi.cmake")
endif()

set(pack_tools "")
if(CONDUIT_DIR)
    list(APPEND pack_tools CONDUIT_DIR "${CONDUIT_DIR}")
endif()
if(SLANGC)
    list(APPEND pack_tools SLANGC "${SLANGC}")
endif()

perlin_api_add_shader_pack(gpu_ui_shaders
    PROGRAMS shaders/ui.slang shaders/blur.slang
    SLOTS shaders/slots.json
    HEADER "${CMAKE_CURRENT_BINARY_DIR}/gpu_ui_pack.hpp" SYMBOL gpu_ui_pack
    OPTIONAL ${pack_tools})

perlin_api_add_plugin(gpu_ui SOURCES gpu_ui.cpp MANIFEST gpu_ui.json)
add_dependencies(gpu_ui gpu_ui_shaders)
target_include_directories(gpu_ui PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
