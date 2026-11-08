include_guard(GLOBAL)

get_filename_component(PERLIN_API_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(PERLIN_API_SCHEMA_DIR "${PERLIN_API_DIR}/api")
set(PERLIN_API_SDK_DIR "${PERLIN_API_DIR}/sdk")
set(PERLIN_API_INCLUDE_DIRS "${PERLIN_API_DIR}/sdk" "${PERLIN_API_DIR}")
set(PERLIN_API_HOST_CORE_SOURCES "${PERLIN_API_DIR}/host/PluginCore.cpp")
set(PERLIN_API_HOST_WASMEDGE_SOURCES "${PERLIN_API_DIR}/host/WasmRuntime.cpp")
set(PERLIN_API_HOST_WASMEDGE_STOP_SOURCES "${PERLIN_API_DIR}/host/WasmEdgeStop.cpp")
set(PERLIN_API_HOST_WASM3_SOURCES "${PERLIN_API_DIR}/host/WasmRuntimeWasm3.cpp")

file(GLOB PERLIN_API_SCHEMA_FILES CONFIGURE_DEPENDS "${PERLIN_API_SCHEMA_DIR}/*.papi")
file(GLOB_RECURSE PERLIN_API_CODEGEN_FILES CONFIGURE_DEPENDS "${PERLIN_API_DIR}/codegen/*.py")

if(NOT TARGET perlin_api)
    add_library(perlin_api INTERFACE)
    target_include_directories(perlin_api INTERFACE ${PERLIN_API_INCLUDE_DIRS})
    target_compile_features(perlin_api INTERFACE cxx_std_23)
    add_library(perlin::api ALIAS perlin_api)
endif()

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_FOUND AND NOT TARGET perlin_api_codegen)
    add_custom_target(perlin_api_codegen
        COMMAND ${Python3_EXECUTABLE} "${PERLIN_API_DIR}/codegen/generate.py"
        WORKING_DIRECTORY "${PERLIN_API_DIR}"
        SOURCES ${PERLIN_API_SCHEMA_FILES}
        COMMENT "Regenerating the Perlin mod ABI from api"
        VERBATIM)
endif()

function(perlin_api_add_checks)
    cmake_parse_arguments(PARSE_ARGV 0 CHK "" "CLIENT;SERVER" "")
    if(Python3_FOUND)
        set(host_args "")
        if(CHK_CLIENT)
            list(APPEND host_args --client "${CHK_CLIENT}")
        endif()
        if(CHK_SERVER)
            list(APPEND host_args --server "${CHK_SERVER}")
        endif()
        add_test(NAME perlin_api_codegen_check
            COMMAND ${Python3_EXECUTABLE} "${PERLIN_API_DIR}/codegen/generate.py" --check)
        add_test(NAME perlin_api_compat_check
            COMMAND ${Python3_EXECUTABLE} "${PERLIN_API_DIR}/tools/check_compat.py")
        add_test(NAME perlin_api_host_check
            COMMAND ${Python3_EXECUTABLE} "${PERLIN_API_DIR}/tools/check_api.py" ${host_args})
    endif()
endfunction()

function(perlin_api_add_plugin target)
    cmake_parse_arguments(PARSE_ARGV 1 PLUGIN "" "MANIFEST;OUTPUT_NAME" "SOURCES")
    get_filename_component(api_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    if(NOT PLUGIN_SOURCES)
        message(FATAL_ERROR "perlin_api_add_plugin(${target}): pass SOURCES <file.cpp>...")
    endif()
    if(NOT EMSCRIPTEN AND NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
        message(FATAL_ERROR "perlin_api_add_plugin(${target}) builds WebAssembly: configure with `emcmake cmake` (Emscripten, wasm64) or with the wasi-sdk toolchain file (wasm32)")
    endif()
    if(NOT PLUGIN_OUTPUT_NAME)
        set(PLUGIN_OUTPUT_NAME "${target}")
    endif()

    add_executable(${target} ${PLUGIN_SOURCES})
    target_link_libraries(${target} PRIVATE perlin::api)
    target_compile_options(${target} PRIVATE -O2 -fno-exceptions -fno-rtti -Wall -Wextra)
    set_target_properties(${target} PROPERTIES OUTPUT_NAME "${PLUGIN_OUTPUT_NAME}" SUFFIX ".wasm")

    set(exports perlin_api_revision perlin_alloc perlin_free perlin_event perlin_subscriptions perlin_subscriptions_hi)
    if(EMSCRIPTEN)
        target_compile_options(${target} PRIVATE -sMEMORY64=1)
        list(JOIN exports ",_" joined)
        target_link_options(${target} PRIVATE
            -sMEMORY64=1 -sSTANDALONE_WASM=1 -sPURE_WASI=1 -sALLOW_MEMORY_GROWTH=1
            -sINITIAL_MEMORY=1MB -sMAXIMUM_MEMORY=1GB -sSTACK_SIZE=256KB
            -sERROR_ON_UNDEFINED_SYMBOLS=0 --no-entry
            "-sEXPORTED_FUNCTIONS=_${joined}")
    else()
        target_compile_options(${target} PRIVATE -mexec-model=reactor)
        target_link_options(${target} PRIVATE -mexec-model=reactor -Wl,--strip-all)
        foreach(name IN LISTS exports)
            target_link_options(${target} PRIVATE "-Wl,--export=${name}")
        endforeach()
    endif()

    if(PLUGIN_MANIFEST)
        get_filename_component(manifest "${PLUGIN_MANIFEST}" ABSOLUTE)
        set(gen_dir "${CMAKE_CURRENT_BINARY_DIR}/perlin_gen/${target}")
        set(gen_header "${gen_dir}/mod_settings.hpp")
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        add_custom_command(OUTPUT "${gen_header}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${gen_dir}"
            COMMAND ${Python3_EXECUTABLE} "${api_dir}/tools/perlin_settings.py" "${manifest}" "${gen_header}"
            DEPENDS "${manifest}" "${api_dir}/tools/perlin_settings.py"
            VERBATIM)
        add_custom_target(${target}_settings DEPENDS "${gen_header}")
        add_dependencies(${target} ${target}_settings)
        target_include_directories(${target} PRIVATE "${gen_dir}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${manifest}"
                    "$<TARGET_FILE_DIR:${target}>/${PLUGIN_OUTPUT_NAME}.json"
            VERBATIM)
    endif()
endfunction()

# Builds a conduit shader pack from Slang programs and embeds it in a header.
#
#   perlin_api_add_shader_pack(my_pack
#       PROGRAMS shaders/ui.slang shaders/blur.slang
#       SLOTS shaders/slots.json
#       HEADER ${CMAKE_CURRENT_BINARY_DIR}/my_pack.hpp SYMBOL my_pack
#       [INCLUDE dir...] [TARGETS spirv,dxil,hlsl,msl,wgsl] [CONDUIT_DIR path] [SLANGC path]
#       [OPTIONAL])    with OPTIONAL a missing tool gives an empty placeholder pack and a warning instead of an error
#
# Or embed a pack someone else built: perlin_api_add_shader_pack(my_pack PREBUILT my.pack HEADER ... SYMBOL ...)
#
# CONDUIT_DIR is a conduit checkout (it provides tools/pack_shaders.py); SLANGC is the Slang compiler
# (the Vulkan SDK ships one). Both can also come from the CONDUIT_DIR and SLANGC environment variables.
# Build the target `my_pack` before the plugin and include the header.
# Writes a header with an empty placeholder pack so a plugin still builds without the shader tools; at run time
# gpu::load_pack rejects it and the plugin can say the shaders are missing.
function(perlin_api_empty_pack target header symbol reason)
    message(WARNING "${target}: ${reason}. Building with an EMPTY shader pack: the plugin will load but cannot draw. "
                    "Pass PREBUILT <file.pack> (from someone who has slangc) or set CONDUIT_DIR and SLANGC.")
    file(WRITE "${header}" "#pragma once\n#include <cstdint>\ninline constexpr uint8_t ${symbol}[] = {0};\n")
    add_custom_target(${target})
endfunction()

function(perlin_api_add_shader_pack target)
    if(POLICY CMP0174)
        cmake_policy(SET CMP0174 NEW)
    endif()
    cmake_parse_arguments(PARSE_ARGV 1 PACK "OPTIONAL" "SLOTS;HEADER;SYMBOL;TARGETS;CONDUIT_DIR;SLANGC;PREBUILT" "PROGRAMS;INCLUDE")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    get_filename_component(api_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    if(NOT PACK_HEADER OR NOT PACK_SYMBOL)
        message(FATAL_ERROR "perlin_api_add_shader_pack(${target}): pass HEADER and SYMBOL")
    endif()
    if(PACK_PREBUILT)
        get_filename_component(prebuilt "${PACK_PREBUILT}" ABSOLUTE)
        add_custom_command(OUTPUT "${PACK_HEADER}"
            COMMAND ${Python3_EXECUTABLE} "${api_dir}/tools/embed_file.py" "${prebuilt}" "${PACK_HEADER}" "${PACK_SYMBOL}"
            DEPENDS "${prebuilt}" "${api_dir}/tools/embed_file.py" VERBATIM)
        add_custom_target(${target} DEPENDS "${PACK_HEADER}")
        return()
    endif()
    if(NOT PACK_PROGRAMS OR NOT PACK_SLOTS)
        message(FATAL_ERROR "perlin_api_add_shader_pack(${target}): pass PROGRAMS and SLOTS (or PREBUILT <file.pack>)")
    endif()
    if(NOT PACK_CONDUIT_DIR AND DEFINED ENV{CONDUIT_DIR})
        set(PACK_CONDUIT_DIR "$ENV{CONDUIT_DIR}")
    endif()
    if(PACK_OPTIONAL AND (NOT PACK_CONDUIT_DIR OR NOT EXISTS "${PACK_CONDUIT_DIR}/tools/pack_shaders.py"))
        perlin_api_empty_pack(${target} "${PACK_HEADER}" "${PACK_SYMBOL}" "no conduit checkout (set CONDUIT_DIR)")
        return()
    endif()
    if(NOT PACK_CONDUIT_DIR OR NOT EXISTS "${PACK_CONDUIT_DIR}/tools/pack_shaders.py")
        message(FATAL_ERROR "perlin_api_add_shader_pack(${target}): the shader pack builder was not found. Clone conduit and pass its folder: -DCONDUIT_DIR=C:/path/to/conduit (the folder that contains tools/pack_shaders.py), or set the CONDUIT_DIR environment variable. Packing also needs slangc: -DSLANGC=C:/path/to/slangc.exe, or install the Vulkan SDK.")
    endif()
    if(NOT PACK_SLANGC AND DEFINED ENV{SLANGC})
        set(PACK_SLANGC "$ENV{SLANGC}")
    endif()
    if(NOT PACK_SLANGC)
        find_program(PACK_SLANGC slangc HINTS ENV VULKAN_SDK PATH_SUFFIXES bin Bin)
    endif()
    if(PACK_OPTIONAL AND NOT PACK_SLANGC)
        perlin_api_empty_pack(${target} "${PACK_HEADER}" "${PACK_SYMBOL}" "slangc not found (set SLANGC or install the Vulkan SDK)")
        return()
    endif()
    if(NOT PACK_SLANGC)
        message(FATAL_ERROR "perlin_api_add_shader_pack(${target}): slangc not found; pass SLANGC or install the Vulkan SDK")
    endif()
    if(NOT PACK_TARGETS)
        set(PACK_TARGETS "spirv,dxil,hlsl,msl,wgsl")
    endif()

    set(programs "")
    foreach(p IN LISTS PACK_PROGRAMS)
        get_filename_component(abs "${p}" ABSOLUTE)
        list(APPEND programs "${abs}")
    endforeach()
    get_filename_component(slots "${PACK_SLOTS}" ABSOLUTE)
    set(include_args "")
    foreach(dir IN LISTS PACK_INCLUDE)
        get_filename_component(abs "${dir}" ABSOLUTE)
        list(APPEND include_args -I "${abs}")
    endforeach()

    set(pack_file "${PACK_HEADER}.pack")
    file(GLOB_RECURSE included CONFIGURE_DEPENDS ${PACK_INCLUDE})
    add_custom_command(
        OUTPUT "${PACK_HEADER}"
        COMMAND ${Python3_EXECUTABLE} "${PACK_CONDUIT_DIR}/tools/pack_shaders.py"
                --slangc "${PACK_SLANGC}" --slots "${slots}" --targets "${PACK_TARGETS}" ${include_args}
                -o "${pack_file}" ${programs}
        COMMAND ${Python3_EXECUTABLE} "${api_dir}/tools/embed_file.py" "${pack_file}" "${PACK_HEADER}" "${PACK_SYMBOL}"
        DEPENDS ${programs} "${slots}" "${PACK_CONDUIT_DIR}/tools/pack_shaders.py" "${api_dir}/tools/embed_file.py"
        COMMENT "Packing shaders for ${target}"
        VERBATIM)
    add_custom_target(${target} DEPENDS "${PACK_HEADER}")
endfunction()
