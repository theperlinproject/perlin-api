#pragma once
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace perlin::gpu::stream {
    constexpr uint32_t Magic = 0x55475050u;
    constexpr uint32_t Version = 1;

    enum class Cmd : uint16_t {
        BeginPass = 1,
        EndPass = 2,
        SetPipeline = 3,
        SetViewport = 4,
        SetScissor = 5,
        SetVertexBuffer = 6,
        SetIndexBuffer = 7,
        SetUniforms = 8,
        BindTexture = 9,
        BindStorage = 10,
        Draw = 11,
        DrawIndexed = 12,
        DrawIndexedIndirect = 13,
        Dispatch = 14,
        CopyBuffer = 15,
        CopyTexture = 16,
        CopyScreen = 17,
        FillBuffer = 18,
        PushLabel = 19,
        PopLabel = 20,
    };

    enum class Load : uint32_t { Load = 0, Clear = 1, DontCare = 2 };
    enum class Store : uint32_t { Store = 0, DontCare = 1 };

    constexpr uint32_t MaxColors = 4;

    struct Header {
        uint32_t magic = Magic;
        uint32_t version = Version;
        uint32_t bytes = 0;
        uint32_t commands = 0;
    };
    static_assert(sizeof(Header) == 16);

    struct Head {
        uint16_t op = 0;
        uint16_t words = 0;
    };
    static_assert(sizeof(Head) == 4);

    struct Color {
        uint32_t texture = 0;
        uint32_t load = static_cast<uint32_t>(Load::Load);
        uint32_t store = static_cast<uint32_t>(Store::Store);
        float clear[4] = {0, 0, 0, 0};
        uint32_t mip = 0;
        uint32_t layer = 0;
        uint32_t resolve = 0;
    };
    static_assert(sizeof(Color) == 40);

    struct Depth {
        uint32_t texture = 0;
        uint32_t load = static_cast<uint32_t>(Load::Clear);
        uint32_t store = static_cast<uint32_t>(Store::Store);
        float clearDepth = 1.0f;
        uint32_t clearStencil = 0;
        uint32_t readOnly = 0;
        uint32_t layer = 0;
    };
    static_assert(sizeof(Depth) == 28);

    struct BeginPass {
        uint32_t colorCount = 0;
        uint32_t hasDepth = 0;
        Color colors[MaxColors];
        Depth depth;
    };
    static_assert(sizeof(BeginPass) == 8 + 40 * 4 + 28);

    struct SetPipeline { uint32_t pipeline = 0; };
    struct SetViewport { float x = 0, y = 0, width = 0, height = 0, minDepth = 0, maxDepth = 1; };
    struct SetScissor { int32_t x = 0, y = 0; uint32_t width = 0, height = 0; };
    struct SetVertexBuffer { uint32_t slot = 0, buffer = 0, offsetLo = 0, offsetHi = 0; };
    struct SetIndexBuffer { uint32_t buffer = 0, wide = 0, offsetLo = 0, offsetHi = 0; };
    struct SetUniforms { uint32_t offset = 0, bytes = 0; };
    struct BindTexture { uint32_t slot = 0, texture = 0, sampler = 0, layer = 0; };
    struct BindStorage { uint32_t binding = 0, resource = 0; };
    struct Draw { uint32_t vertexCount = 0, instanceCount = 1, firstVertex = 0, firstInstance = 0; };
    struct DrawIndexed {
        uint32_t indexCount = 0, instanceCount = 1, firstIndex = 0;
        int32_t vertexOffset = 0;
        uint32_t firstInstance = 0;
    };
    struct DrawIndexedIndirect { uint32_t buffer = 0, offsetLo = 0, offsetHi = 0, maxDraws = 1, stride = 20; };
    struct Dispatch { uint32_t x = 1, y = 1, z = 1; };
    struct CopyBuffer {
        uint32_t dst = 0, src = 0;
        uint32_t dstOffsetLo = 0, dstOffsetHi = 0, srcOffsetLo = 0, srcOffsetHi = 0, sizeLo = 0, sizeHi = 0;
    };
    struct CopyTexture {
        uint32_t dst = 0, src = 0;
        uint32_t dstX = 0, dstY = 0, srcX = 0, srcY = 0, width = 0, height = 0;
        uint32_t dstLayer = 0, srcLayer = 0, dstMip = 0, srcMip = 0;
    };
    struct CopyScreen { uint32_t dst = 0; };
    struct FillBuffer { uint32_t dst = 0, value = 0, offsetLo = 0, offsetHi = 0, sizeLo = 0, sizeHi = 0; };
    struct PushLabel { uint32_t length = 0; };

    class Writer {
    public:
        Writer() { bytes_.resize(sizeof(Header)); }

        void clear() {
            bytes_.resize(sizeof(Header));
            count_ = 0;
        }
        uint32_t commands() const { return count_; }

        std::span<const uint8_t> finish() {
            Header h;
            h.bytes = static_cast<uint32_t>(bytes_.size());
            h.commands = count_;
            std::memcpy(bytes_.data(), &h, sizeof(h));
            return {bytes_.data(), bytes_.size()};
        }

        template <class T> void add(Cmd op, const T& payload) { add(op, &payload, sizeof(T), nullptr, 0); }
        void add(Cmd op) { add(op, nullptr, 0, nullptr, 0); }

        void add(Cmd op, const void* payload, size_t payloadBytes, const void* tail, size_t tailBytes) {
            const size_t body = payloadBytes + tailBytes;
            const size_t padded = (body + 3) & ~size_t{3};
            const size_t words = (sizeof(Head) + padded) / 4;
            Head head;
            head.op = static_cast<uint16_t>(op);
            head.words = static_cast<uint16_t>(words);
            const size_t at = bytes_.size();
            bytes_.resize(at + words * 4, 0);
            std::memcpy(bytes_.data() + at, &head, sizeof(head));
            if (payloadBytes)
                std::memcpy(bytes_.data() + at + sizeof(head), payload, payloadBytes);
            if (tailBytes)
                std::memcpy(bytes_.data() + at + sizeof(head) + payloadBytes, tail, tailBytes);
            count_++;
        }

    private:
        std::vector<uint8_t> bytes_;
        uint32_t count_ = 0;
    };

    struct Command {
        Cmd op = Cmd::EndPass;
        const uint8_t* payload = nullptr;
        size_t bytes = 0;

        template <class T> bool read(T& out) const {
            if (bytes < sizeof(T))
                return false;
            std::memcpy(&out, payload, sizeof(T));
            return true;
        }
        std::span<const uint8_t> tail(size_t afterPayload) const {
            if (bytes < afterPayload)
                return {};
            return {payload + afterPayload, bytes - afterPayload};
        }
    };

    class Reader {
    public:
        explicit Reader(std::span<const uint8_t> data) : data_(data) {
            if (data.size() < sizeof(Header)) {
                ok_ = false;
                return;
            }
            std::memcpy(&header_, data.data(), sizeof(Header));
            ok_ = header_.magic == Magic && header_.version == Version && header_.bytes == data.size() &&
                  (data.size() & 3) == 0;
            at_ = sizeof(Header);
        }

        bool valid() const { return ok_; }
        uint32_t commands() const { return header_.commands; }

        bool next(Command& out) {
            if (!ok_ || seen_ >= header_.commands)
                return false;
            if (at_ + sizeof(Head) > data_.size()) {
                ok_ = false;
                return false;
            }
            Head head;
            std::memcpy(&head, data_.data() + at_, sizeof(head));
            const size_t size = static_cast<size_t>(head.words) * 4;
            if (size < sizeof(Head) || at_ + size > data_.size()) {
                ok_ = false;
                return false;
            }
            out.op = static_cast<Cmd>(head.op);
            out.payload = data_.data() + at_ + sizeof(Head);
            out.bytes = size - sizeof(Head);
            at_ += size;
            seen_++;
            return true;
        }

        bool complete() const { return ok_ && seen_ == header_.commands && at_ == data_.size(); }

    private:
        std::span<const uint8_t> data_;
        Header header_;
        size_t at_ = 0;
        uint32_t seen_ = 0;
        bool ok_ = true;
    };

    constexpr uint64_t join(uint32_t lo, uint32_t hi) {
        return static_cast<uint64_t>(lo) | (static_cast<uint64_t>(hi) << 32);
    }
}
