cmake_minimum_required(VERSION 3.28)
project(gpu_ui LANGUAGES CXX)

# emcmake cmake -S . -B build -DCONDUIT_DIR=/path/to/conduit [-DSLANGC=/path/to/slangc]
include(FetchContent)
if(NOT TARGET perlin::api)
    FetchContent_Declare(perlin_api GIT_REPOSITORY https://github.com/theperlinproject/perlin-api.git GIT_TAG main)
    FetchContent_MakeAvailable(perlin_api)
    include("${perlin_api_SOURCE_DIR}/cmake/PerlinApi.cmake")
endif()

perlin_api_add_shader_pack(gpu_ui_shaders
    PROGRAMS shaders/ui.slang shaders/blur.slang
    SLOTS shaders/slots.json
    HEADER "${CMAKE_CURRENT_BINARY_DIR}/gpu_ui_pack.hpp" SYMBOL gpu_ui_pack
    CONDUIT_DIR "${CONDUIT_DIR}" SLANGC "${SLANGC}")

perlin_api_add_plugin(gpu_ui SOURCES gpu_ui.cpp MANIFEST gpu_ui.json)
add_dependencies(gpu_ui gpu_ui_shaders)
target_include_directories(gpu_ui PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
