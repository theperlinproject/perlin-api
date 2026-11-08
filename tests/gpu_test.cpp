#include <cstdio>

#include <perlin/gpu.hpp>

using namespace perlin;

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x); failures++; } } while (0)

int main() {
    gpu::stream::Writer w;
    gpu::stream::BeginPass pass;
    pass.colorCount = 1;
    pass.colors[0].load = static_cast<uint32_t>(gpu::stream::Load::Clear);
    pass.colors[0].clear[3] = 0.5f;
    w.add(gpu::stream::Cmd::BeginPass, pass);
    w.add(gpu::stream::Cmd::SetPipeline, gpu::stream::SetPipeline{7});
    const uint8_t uniforms[6] = {1, 2, 3, 4, 5, 6};
    gpu::stream::SetUniforms u{16, 6};
    w.add(gpu::stream::Cmd::SetUniforms, &u, sizeof(u), uniforms, sizeof(uniforms));
    w.add(gpu::stream::Cmd::Draw, gpu::stream::Draw{3, 1, 0, 0});
    w.add(gpu::stream::Cmd::EndPass);
    const auto bytes = w.finish();

    gpu::stream::Reader r(bytes);
    CHECK(r.valid());
    CHECK(r.commands() == 5);
    gpu::stream::Command c;
    CHECK(r.next(c) && c.op == gpu::stream::Cmd::BeginPass);
    gpu::stream::BeginPass back;
    CHECK(c.read(back) && back.colors[0].clear[3] == 0.5f && back.colorCount == 1);
    CHECK(r.next(c) && c.op == gpu::stream::Cmd::SetPipeline);
    gpu::stream::SetPipeline sp;
    CHECK(c.read(sp) && sp.pipeline == 7);
    CHECK(r.next(c) && c.op == gpu::stream::Cmd::SetUniforms);
    gpu::stream::SetUniforms su;
    CHECK(c.read(su) && su.offset == 16 && su.bytes == 6);
    const auto tail = c.tail(sizeof(su));
    CHECK(tail.size() >= 6 && tail[0] == 1 && tail[5] == 6);
    CHECK(r.next(c) && c.op == gpu::stream::Cmd::Draw);
    CHECK(r.next(c) && c.op == gpu::stream::Cmd::EndPass);
    CHECK(!r.next(c));
    CHECK(r.complete());

    std::vector<uint8_t> bad(bytes.begin(), bytes.end());
    bad.resize(bad.size() - 4);
    CHECK(!gpu::stream::Reader(bad).valid());
    std::vector<uint8_t> corrupt(bytes.begin(), bytes.end());
    corrupt[sizeof(gpu::stream::Header) + 2] = 0xFF;
    corrupt[sizeof(gpu::stream::Header) + 3] = 0xFF;
    gpu::stream::Reader cr(corrupt);
    CHECK(cr.valid());
    CHECK(!cr.next(c));
    CHECK(!cr.valid());

    gpu::Commands cmds;
    cmds.begin_pass_screen().viewport(0, 0, 10, 10).scissor(0, 0, 10, 10).draw(3).end_pass();
    CHECK(cmds.size() == 5);

    gpu::QuadBatch batch;
    batch.rect(0, 0, 4, 4, 0xFFFFFFFFu);
    CHECK(batch.vertices().size() == 4 && batch.index_count() == 6);

    abi::GpuProgramInfo program;
    program.name = "ui";
    program.uniformBytes = 32;
    program.uniforms = {{"u_a", 0, 16, 1}, {"u_b", 16, 8, 1}};
    gpu::Uniforms block(program);
    CHECK(block.set("u_a", std::array<float, 4>{1, 2, 3, 4}));
    CHECK(block.set("u_b", std::array<float, 2>{5, 6}));
    CHECK(!block.set("u_c", 1.0f));
    CHECK(!block.set("u_b", std::array<float, 4>{}));
    float second;
    std::memcpy(&second, block.bytes().data() + 16, 4);
    CHECK(second == 5.0f);
    CHECK(block.bytes().size() == 32);

    std::printf(failures ? "gpu_test: %d failure(s)\n" : "gpu_test: ok\n", failures);
    return failures ? 1 : 0;
}
