#pragma once
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "perlin/gpu_stream.hpp"
#include "perlin/rpc.hpp"
#include "perlin/types.hpp"

namespace perlin::gpu {
    using abi::GpuAddress;
    using abi::GpuBlendDesc;
    using abi::GpuBlendFactor;
    using abi::GpuBlendOp;
    using abi::GpuBufferUsage;
    using abi::GpuCompare;
    using abi::GpuCull;
    using abi::GpuFilter;
    using abi::GpuFormat;
    using abi::GpuFrontFace;
    using abi::GpuInfoData;
    using abi::GpuPackInfo;
    using abi::GpuProgramInfo;
    using abi::GpuResource;
    using abi::GpuStage;
    using abi::GpuStorageKind;
    using abi::GpuTextureDim;
    using abi::GpuTextureUsage;
    using abi::GpuTopology;

    constexpr uint32_t screen = 0;

    using BufferUsage = meta::Flags<GpuBufferUsage>;
    using TextureUsage = meta::Flags<GpuTextureUsage>;

    constexpr BufferUsage buffer_usage(std::initializer_list<GpuBufferUsage> bits) {
        BufferUsage v;
        for (GpuBufferUsage b : bits)
            v.set(b);
        return v;
    }

    constexpr TextureUsage texture_usage(std::initializer_list<GpuTextureUsage> bits) {
        TextureUsage v;
        for (GpuTextureUsage b : bits)
            v.set(b);
        return v;
    }

    inline Result<GpuInfoData> info() {
        return call<abi::Op::GpuInfo>();
    }

    class Pack {
    public:
        Pack() = default;
        explicit Pack(GpuPackInfo info) : info_(std::move(info)) {}

        uint32_t id() const { return info_.pack; }
        const GpuPackInfo& info() const { return info_; }

        const GpuProgramInfo* program(std::string_view name) const {
            for (const GpuProgramInfo& p : info_.programs)
                if (p.name == name)
                    return &p;
            return nullptr;
        }

        int slot(std::string_view name) const {
            for (const abi::GpuTextureSlot& s : info_.slots)
                if (s.name == name)
                    return static_cast<int>(s.slot);
            return -1;
        }

    private:
        GpuPackInfo info_;
    };

    inline Result<Pack> load_pack(std::span<const uint8_t> data) {
        abi::GpuPackLoadRequest r;
        r.data.assign(data.begin(), data.end());
        auto info = call<abi::Op::GpuPackLoad>(r);
        if (!info)
            return unexpected(info.error());
        return Pack(std::move(*info));
    }

    inline Result<uint32_t> create_buffer(uint64_t size, BufferUsage usage, std::span<const uint8_t> data = {},
                                          std::string name = {}) {
        abi::GpuBufferRequest r;
        r.size = size ? size : data.size();
        r.usage = usage;
        r.data.assign(data.begin(), data.end());
        r.name = std::move(name);
        return call<abi::Op::GpuBufferCreate>(r);
    }

    template <class T>
    Result<uint32_t> create_buffer(std::span<const T> data, BufferUsage usage, std::string name = {}) {
        return create_buffer(0, usage,
                             std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size_bytes()),
                             std::move(name));
    }

    inline Result<void> write_buffer(uint32_t id, uint64_t offset, std::span<const uint8_t> data) {
        abi::GpuBufferWriteRequest r;
        r.id = id;
        r.offset = offset;
        r.data.assign(data.begin(), data.end());
        return call<abi::Op::GpuBufferWrite>(r);
    }

    template <class T> Result<void> write_buffer(uint32_t id, uint64_t offset, std::span<const T> data) {
        return write_buffer(id, offset,
                            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size_bytes()));
    }

    inline Result<void> read_buffer(uint32_t id, uint64_t offset, uint64_t size, uint32_t tag) {
        return call<abi::Op::GpuBufferRead>(abi::GpuBufferReadRequest{id, offset, size, tag});
    }

    struct TextureOptions {
        GpuFormat format = GpuFormat::Rgba8;
        TextureUsage usage;
        GpuTextureDim dim = GpuTextureDim::Tex2d;
        uint32_t layers = 1;
        uint32_t mips = 1;
        uint32_t samples = 1;
        std::string name;
    };

    inline Result<uint32_t> create_texture(uint32_t width, uint32_t height, TextureOptions options = {},
                                           std::span<const uint8_t> data = {}) {
        abi::GpuTextureRequest r;
        r.width = width;
        r.height = height;
        r.format = options.format;
        r.usage = !options.usage.empty() ? options.usage : texture_usage({GpuTextureUsage::Sampled, GpuTextureUsage::CopyDst});
        r.dim = options.dim;
        r.layers = options.layers;
        r.mips = options.mips;
        r.samples = options.samples;
        r.name = std::move(options.name);
        r.data.assign(data.begin(), data.end());
        return call<abi::Op::GpuTextureCreate>(r);
    }

    inline Result<uint32_t> create_screen_texture(GpuFormat format = GpuFormat::Screen, float scale = 1.0f,
                                                  TextureUsage usage = {}, std::string name = {}) {
        abi::GpuTextureRequest r;
        r.format = format;
        r.screenSized = true;
        r.scale = scale;
        r.usage = !usage.empty() ? usage
                        : texture_usage({GpuTextureUsage::Sampled, GpuTextureUsage::ColorTarget, GpuTextureUsage::CopyDst});
        r.name = std::move(name);
        return call<abi::Op::GpuTextureCreate>(r);
    }

    inline Result<void> write_texture(uint32_t id, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                      std::span<const uint8_t> data, uint32_t layer = 0, uint32_t mip = 0) {
        abi::GpuTextureWriteRequest r;
        r.id = id;
        r.layer = layer;
        r.mip = mip;
        r.x = x;
        r.y = y;
        r.width = width;
        r.height = height;
        r.data.assign(data.begin(), data.end());
        return call<abi::Op::GpuTextureWrite>(r);
    }

    inline Result<uint32_t> create_sampler(abi::GpuSamplerRequest request = {}) {
        return call<abi::Op::GpuSamplerCreate>(request);
    }

    inline Result<uint32_t> linear_sampler(GpuAddress address = GpuAddress::Clamp) {
        abi::GpuSamplerRequest r;
        r.u = r.v = r.w = address;
        return create_sampler(r);
    }

    inline Result<uint32_t> nearest_sampler(GpuAddress address = GpuAddress::Clamp) {
        abi::GpuSamplerRequest r;
        r.min = r.mag = r.mip = GpuFilter::Nearest;
        r.u = r.v = r.w = address;
        return create_sampler(r);
    }

    inline abi::GpuBlendDesc alpha_blend() {
        abi::GpuBlendDesc b;
        b.enabled = true;
        b.srcColor = GpuBlendFactor::SrcAlpha;
        b.dstColor = GpuBlendFactor::InvSrcAlpha;
        b.srcAlpha = GpuBlendFactor::One;
        b.dstAlpha = GpuBlendFactor::InvSrcAlpha;
        return b;
    }

    inline abi::GpuBlendDesc premultiplied_blend() {
        abi::GpuBlendDesc b;
        b.enabled = true;
        b.srcColor = GpuBlendFactor::One;
        b.dstColor = GpuBlendFactor::InvSrcAlpha;
        b.srcAlpha = GpuBlendFactor::One;
        b.dstAlpha = GpuBlendFactor::InvSrcAlpha;
        return b;
    }

    inline Result<uint32_t> create_pipeline(const abi::GpuPipelineRequest& request) {
        return call<abi::Op::GpuPipelineCreate>(request);
    }

    inline Result<uint32_t> create_compute_pipeline(const Pack& pack, std::string program,
                                                    std::vector<abi::GpuStorageBinding> storage = {}) {
        abi::GpuComputePipelineRequest r;
        r.pack = pack.id();
        r.program = std::move(program);
        r.storage = std::move(storage);
        return call<abi::Op::GpuComputePipelineCreate>(r);
    }

    inline Result<void> destroy(GpuResource kind, uint32_t id) {
        return call<abi::Op::GpuDestroy>(abi::GpuDestroyRequest{kind, id});
    }

    inline Result<void> clear_layer(std::string layer) {
        return call<abi::Op::GpuLayerClear>(abi::GpuLayerClearRequest{std::move(layer)});
    }

    inline Result<std::vector<std::string>> errors() {
        auto r = call<abi::Op::GpuErrors>();
        if (!r)
            return unexpected(r.error());
        return std::move(r->messages);
    }

    class Uniforms {
    public:
        Uniforms() = default;
        explicit Uniforms(const GpuProgramInfo& program) : program_(&program), data_(program.uniformBytes, 0) {}

        bool has(std::string_view member) const { return find(member) != nullptr; }

        template <class T> bool set(std::string_view member, const T& value) {
            static_assert(std::is_trivially_copyable_v<T>);
            const abi::GpuUniformMember* m = find(member);
            if (!m || sizeof(T) > m->bytes)
                return false;
            std::memcpy(data_.data() + m->offset, &value, sizeof(T));
            return true;
        }

        template <class T> bool set(std::string_view member, std::span<const T> values) {
            const abi::GpuUniformMember* m = find(member);
            if (!m || values.size_bytes() > m->bytes)
                return false;
            std::memcpy(data_.data() + m->offset, values.data(), values.size_bytes());
            return true;
        }

        std::span<const uint8_t> bytes() const { return {data_.data(), data_.size()}; }

    private:
        const abi::GpuUniformMember* find(std::string_view member) const {
            if (!program_)
                return nullptr;
            for (const abi::GpuUniformMember& m : program_->uniforms)
                if (m.name == member)
                    return &m;
            return nullptr;
        }

        const GpuProgramInfo* program_ = nullptr;
        std::vector<uint8_t> data_;
    };

    class Commands {
    public:
        Commands& begin_pass_screen(bool clear = false, float r = 0, float g = 0, float b = 0, float a = 0) {
            stream::BeginPass p;
            p.colorCount = 1;
            p.colors[0].texture = screen;
            p.colors[0].load = static_cast<uint32_t>(clear ? stream::Load::Clear : stream::Load::Load);
            p.colors[0].clear[0] = r;
            p.colors[0].clear[1] = g;
            p.colors[0].clear[2] = b;
            p.colors[0].clear[3] = a;
            w_.add(stream::Cmd::BeginPass, p);
            return *this;
        }

        Commands& begin_pass(uint32_t color_texture, bool clear = true, float r = 0, float g = 0, float b = 0,
                             float a = 0) {
            stream::BeginPass p;
            p.colorCount = 1;
            p.colors[0].texture = color_texture;
            p.colors[0].load = static_cast<uint32_t>(clear ? stream::Load::Clear : stream::Load::Load);
            p.colors[0].clear[0] = r;
            p.colors[0].clear[1] = g;
            p.colors[0].clear[2] = b;
            p.colors[0].clear[3] = a;
            w_.add(stream::Cmd::BeginPass, p);
            return *this;
        }

        Commands& begin_pass(const stream::BeginPass& pass) {
            w_.add(stream::Cmd::BeginPass, pass);
            return *this;
        }

        Commands& end_pass() {
            w_.add(stream::Cmd::EndPass);
            return *this;
        }

        Commands& pipeline(uint32_t id) {
            w_.add(stream::Cmd::SetPipeline, stream::SetPipeline{id});
            return *this;
        }

        Commands& viewport(float x, float y, float width, float height, float min_depth = 0, float max_depth = 1) {
            w_.add(stream::Cmd::SetViewport, stream::SetViewport{x, y, width, height, min_depth, max_depth});
            return *this;
        }

        Commands& scissor(int x, int y, uint32_t width, uint32_t height) {
            w_.add(stream::Cmd::SetScissor, stream::SetScissor{x, y, width, height});
            return *this;
        }

        Commands& vertex_buffer(uint32_t slot, uint32_t buffer, uint64_t offset = 0) {
            w_.add(stream::Cmd::SetVertexBuffer,
                   stream::SetVertexBuffer{slot, buffer, static_cast<uint32_t>(offset), static_cast<uint32_t>(offset >> 32)});
            return *this;
        }

        Commands& index_buffer(uint32_t buffer, bool wide = false, uint64_t offset = 0) {
            w_.add(stream::Cmd::SetIndexBuffer,
                   stream::SetIndexBuffer{buffer, wide ? 1u : 0u, static_cast<uint32_t>(offset),
                                          static_cast<uint32_t>(offset >> 32)});
            return *this;
        }

        Commands& uniforms(std::span<const uint8_t> data, uint32_t offset = 0) {
            stream::SetUniforms u{offset, static_cast<uint32_t>(data.size())};
            w_.add(stream::Cmd::SetUniforms, &u, sizeof(u), data.data(), data.size());
            return *this;
        }

        Commands& uniforms(const Uniforms& block) {
            return uniforms(block.bytes());
        }

        Commands& texture(uint32_t slot, uint32_t texture_id, uint32_t sampler_id = 0, uint32_t layer = 0) {
            w_.add(stream::Cmd::BindTexture, stream::BindTexture{slot, texture_id, sampler_id, layer});
            return *this;
        }

        Commands& storage(uint32_t binding, uint32_t resource) {
            w_.add(stream::Cmd::BindStorage, stream::BindStorage{binding, resource});
            return *this;
        }

        Commands& draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0,
                       uint32_t first_instance = 0) {
            w_.add(stream::Cmd::Draw, stream::Draw{vertex_count, instance_count, first_vertex, first_instance});
            return *this;
        }

        Commands& draw_indexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0,
                               int32_t vertex_offset = 0, uint32_t first_instance = 0) {
            w_.add(stream::Cmd::DrawIndexed,
                   stream::DrawIndexed{index_count, instance_count, first_index, vertex_offset, first_instance});
            return *this;
        }

        Commands& draw_indexed_indirect(uint32_t buffer, uint64_t offset, uint32_t max_draws, uint32_t stride = 20) {
            w_.add(stream::Cmd::DrawIndexedIndirect,
                   stream::DrawIndexedIndirect{buffer, static_cast<uint32_t>(offset), static_cast<uint32_t>(offset >> 32),
                                               max_draws, stride});
            return *this;
        }

        Commands& dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1) {
            w_.add(stream::Cmd::Dispatch, stream::Dispatch{x, y, z});
            return *this;
        }

        Commands& copy_buffer(uint32_t dst, uint64_t dst_offset, uint32_t src, uint64_t src_offset, uint64_t size) {
            stream::CopyBuffer c;
            c.dst = dst;
            c.src = src;
            c.dstOffsetLo = static_cast<uint32_t>(dst_offset);
            c.dstOffsetHi = static_cast<uint32_t>(dst_offset >> 32);
            c.srcOffsetLo = static_cast<uint32_t>(src_offset);
            c.srcOffsetHi = static_cast<uint32_t>(src_offset >> 32);
            c.sizeLo = static_cast<uint32_t>(size);
            c.sizeHi = static_cast<uint32_t>(size >> 32);
            w_.add(stream::Cmd::CopyBuffer, c);
            return *this;
        }

        Commands& copy_texture(const stream::CopyTexture& c) {
            w_.add(stream::Cmd::CopyTexture, c);
            return *this;
        }

        Commands& copy_screen(uint32_t dst_texture) {
            w_.add(stream::Cmd::CopyScreen, stream::CopyScreen{dst_texture});
            return *this;
        }

        Commands& fill_buffer(uint32_t dst, uint64_t offset, uint64_t size, uint32_t value = 0) {
            stream::FillBuffer f;
            f.dst = dst;
            f.value = value;
            f.offsetLo = static_cast<uint32_t>(offset);
            f.offsetHi = static_cast<uint32_t>(offset >> 32);
            f.sizeLo = static_cast<uint32_t>(size);
            f.sizeHi = static_cast<uint32_t>(size >> 32);
            w_.add(stream::Cmd::FillBuffer, f);
            return *this;
        }

        Commands& push_label(std::string_view text) {
            stream::PushLabel l{static_cast<uint32_t>(text.size())};
            w_.add(stream::Cmd::PushLabel, &l, sizeof(l), text.data(), text.size());
            return *this;
        }

        Commands& pop_label() {
            w_.add(stream::Cmd::PopLabel);
            return *this;
        }

        void clear() { w_.clear(); }
        uint32_t size() const { return w_.commands(); }

        Result<void> submit(std::string layer, GpuStage stage = GpuStage::Overlay, int z = 0, bool once = false) {
            abi::GpuSubmitRequest r;
            r.layer = std::move(layer);
            r.stage = stage;
            r.z = z;
            r.once = once;
            const auto bytes = w_.finish();
            r.stream.assign(bytes.begin(), bytes.end());
            return call<abi::Op::GpuSubmit>(r);
        }

    private:
        stream::Writer w_;
    };

    struct Vertex2D {
        float x = 0, y = 0;
        float u = 0, v = 0;
        uint32_t color = 0xFFFFFFFFu;
    };

    class QuadBatch {
    public:
        void clear() {
            vertices_.clear();
            indices_.clear();
        }

        void quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, uint32_t color) {
            const uint32_t base = static_cast<uint32_t>(vertices_.size());
            vertices_.push_back({x0, y0, u0, v0, color});
            vertices_.push_back({x1, y0, u1, v0, color});
            vertices_.push_back({x1, y1, u1, v1, color});
            vertices_.push_back({x0, y1, u0, v1, color});
            for (uint32_t i : {0u, 1u, 2u, 0u, 2u, 3u})
                indices_.push_back(base + i);
        }

        void rect(float x0, float y0, float x1, float y1, uint32_t color) {
            quad(x0, y0, x1, y1, 0, 0, 1, 1, color);
        }

        std::span<const Vertex2D> vertices() const { return {vertices_.data(), vertices_.size()}; }
        std::span<const uint32_t> indices() const { return {indices_.data(), indices_.size()}; }
        uint32_t index_count() const { return static_cast<uint32_t>(indices_.size()); }

    private:
        std::vector<Vertex2D> vertices_;
        std::vector<uint32_t> indices_;
    };

    inline abi::GpuPipelineRequest quad_pipeline(const Pack& pack, std::string program, bool blend = true) {
        abi::GpuPipelineRequest r;
        r.pack = pack.id();
        r.program = std::move(program);
        r.streams = {{sizeof(Vertex2D), false}};
        r.attributes = {{0, 0, GpuFormat::Rg32f, 0}, {1, 0, GpuFormat::Rg32f, 8}, {2, 0, GpuFormat::Rgba8, 16}};
        if (blend)
            r.blend = {alpha_blend()};
        r.colorFormats = {GpuFormat::Screen};
        return r;
    }
}
