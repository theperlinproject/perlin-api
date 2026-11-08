# Building plugins

A plugin is a core WebAssembly module (no component model) that imports `perlin.call` and exports `perlin_alloc`, `perlin_free`, `perlin_event` and `perlin_subscriptions`. The headers in `sdk/perlin` provide those exports, so a C++ plugin only needs the right target and flags; `build.sh` has them.

## build.sh

```
./build.sh [--wasm32|--wasm64] <plugin.cpp> [outdir]
```

* Run it from anywhere; the source path is relative to where you are. The output directory defaults to `./build`.
* Headers next to the source are found with `#include <dir/header.hpp>` (the source's own directory is on the include path).
* If `<plugin>.json` sits next to the source, it is copied to the output directory and its `settings` are turned into a typed header, `gen/<plugin>/mod_settings.hpp`, which the source includes as `<mod_settings.hpp>` (see [Settings](settings.md)). This step needs Python 3.

## build.cmd (Windows)

`build.cmd` takes the same arguments and does the same as `build.sh`, from a normal Command Prompt:

```
call C:\emsdk\emsdk_env.bat
perlin-api\build.cmd my_plugin.cpp
set WASI_SDK_PATH=C:\wasi-sdk
perlin-api\build.cmd --wasm32 my_plugin.cpp out
```

Python 3 (`python` or `py -3` on the path) is needed only when a `<plugin>.json` with settings sits next to the source.

## wasm64 with Emscripten (default)

64-bit WebAssembly (memory64) is the default. Install [Emscripten](https://emscripten.org), activate it, and build:

```
source /path/to/emsdk/emsdk_env.sh
./build.sh my_plugin.cpp
```

The script compiles with `-std=c++23 -fno-exceptions -fno-rtti -sMEMORY64` as a standalone WASI reactor (no JavaScript). `EMSDK` is also enough: the script finds `em++` under it.

## wasm32 with wasi-sdk

32-bit modules work everywhere, including hosts without 64-bit memory. Install [wasi-sdk](https://github.com/WebAssembly/wasi-sdk) (version 25 or newer; it has no 64-bit target), set `WASI_SDK_PATH`, and pass `--wasm32`:

```
WASI_SDK_PATH=/opt/wasi-sdk ./build.sh --wasm32 my_plugin.cpp
```

Any compiler that targets `wasm64` or `wasm32-wasip1` and exports `perlin_alloc`, `perlin_free`, `perlin_event` and `perlin_subscriptions` works; the headers compile unchanged for both.

## Plain C

C gets the whole API from `perlin.h` (freestanding C99, no libc). A system `clang` is enough:

```
clang --target=wasm64 -O2 -nostdlib -ffreestanding -fno-builtin -fvisibility=hidden \
  -Wl,--no-entry -Wl,--export-dynamic -Wl,--allow-undefined -Wl,-z,stack-size=65536 \
  -I path/to/perlin-api/sdk -o plugin.wasm plugin.c
```

Use `--target=wasm32` for a 32-bit module.

## Building from your own CMake project

`perlin_api_add_plugin` does what `build.sh` does, so a plugin project needs no script. Pull perlin-api in, then declare the plugin:

```cmake
cmake_minimum_required(VERSION 3.24)
project(my_plugin LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(perlin_api
    GIT_REPOSITORY https://github.com/theperlinproject/perlin-api.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR no-cmake-project)
FetchContent_MakeAvailable(perlin_api)
include("${perlin_api_SOURCE_DIR}/cmake/PerlinApi.cmake")

perlin_api_add_plugin(my_plugin
    SOURCES my_plugin.cpp
    MANIFEST my_plugin.json)
```

Configure with the WebAssembly toolchain, then build:

```
emcmake cmake -B build
cmake --build build
```

For a 32-bit module use the wasi-sdk toolchain instead: `cmake -B build -DCMAKE_TOOLCHAIN_FILE=$WASI_SDK_PATH/share/cmake/wasi-sdk.cmake`.

The result is `build/my_plugin.wasm`. Arguments:

| Argument | Meaning |
| --- | --- |
| `SOURCES` | The plugin's source files |
| `MANIFEST` | Optional `name.json`. Its `settings` become `<mod_settings.hpp>` for the sources, and the file is copied next to the `.wasm` |
| `OUTPUT_NAME` | File name without `.wasm`, when it should differ from the target name |

Configuring with a native compiler stops with a message that says to use Emscripten or wasi-sdk. For a project that is not CMake, `-I<perlin-api>/sdk` is all the headers need.

## Installing and running

Drop `name.wasm` (and optionally `name.json` and `name.png`) into `<data dir>/mods/`. Plugins hot-reload when the file changes (Options → Hot Reload On Save) and can be enabled, disabled and reloaded from **Pause → Mods**. A plugin that asks for dangerous permissions needs approval first, see [Manifest and permissions](manifest.md).
