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
