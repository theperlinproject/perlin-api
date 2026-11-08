#include "host/PluginCore.hpp"
#include "host/WasmRuntime.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace perlin::mods;

static uint32_t RevisionOf(const std::string& path, bool& ok) {
    std::ifstream f(path, std::ios::binary);
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    WasmInstance instance;
    std::string error;
    const WasmInstance::Resolver resolver = [](const std::string&, const std::string&) -> WasmInstance::HostFn {
        return [](WasmInstance::HostContext&, const uint64_t*, uint32_t, uint64_t* results, uint32_t resultCount) {
            for (uint32_t i = 0; i < resultCount; i++)
                results[i] = 0;
            return true;
        };
    };
    ok = !bytes.empty() && instance.Load(bytes.data(), bytes.size(), resolver, error);
    if (!ok) {
        std::printf("load %s: %s\n", path.c_str(), error.c_str());
        return 0;
    }
    return ReadApiRevision(instance);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::printf("usage: host_revision_test <data dir> <current revision>\n");
        return 2;
    }
    const std::string dir = argv[1];
    const uint32_t current = static_cast<uint32_t>(std::stoul(argv[2]));
    int failures = 0;
    bool ok = false;
    const uint32_t built = RevisionOf(dir + "/hello_rev4.wasm", ok);
    if (!ok || built != current) {
        std::printf("FAIL: a plugin built with the current headers declares revision %u, expected %u\n", built, current);
        failures++;
    }
    const uint32_t legacy = RevisionOf(dir + "/hello_legacy32.wasm", ok);
    if (!ok || legacy != kLegacyApiRevision) {
        std::printf("FAIL: a plugin without the export reports revision %u, expected %u\n", legacy, kLegacyApiRevision);
        failures++;
    }
    std::printf("host_revision_test: current-headers plugin = %u, legacy plugin = %u\n", built, legacy);
    return failures ? 1 : 0;
}
