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
    const uint32_t newer = RevisionOf(dir + "/hello_rev2.wasm", ok);
    if (!ok || newer != current + 1) {
        std::printf("FAIL: a plugin built for a newer revision declares %u, expected %u\n", newer, current + 1);
        failures++;
    }
    const uint32_t undeclared = RevisionOf(dir + "/hello_undeclared32.wasm", ok);
    if (!ok || undeclared != kDefaultApiRevision) {
        std::printf("FAIL: a plugin without the export reports revision %u, expected %u\n", undeclared, kDefaultApiRevision);
        failures++;
    }
    std::printf("host_revision_test: newer plugin = %u, plugin without the export = %u\n", newer, undeclared);
    return failures ? 1 : 0;
}
