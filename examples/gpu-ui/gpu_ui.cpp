// A frosted-glass panel behind the HUD (UnderHud) and a coloured bar in front of everything (Overlay),
// drawn with the plugin's own shader pack.
//
// Build the shader pack first (see CMakeLists.txt); it is embedded as gpu_ui_pack.hpp.

#include <perlin/perlin.hpp>

#include <algorithm>
#include <array>

#include "gpu_ui_pack.hpp"

using namespace perlin;

class GpuUi : public Plugin {
public:
    void onEnable(Context&) {
        build();
    }

    // The renderer was recreated (graphics API switch, device lost): every id is gone.
    void onGpuReset() {
        ready_ = false;
        failed_ = false;
        build();
    }

    void onFrame() {
        auto info = gpu::info();
        if (!info || !info->available)
            return;
        if (!ready_) {
            if (!failed_)
                build();
            return;
        }
        if (info->width != width_ || info->height != height_)
            layout(info->width, info->height);
    }

private:
    static constexpr float kScale = 0.5f; // the blur runs at half resolution

    void build() {
        failed_ = true; // report a problem once; a renderer reset gives it another try
        auto info = gpu::info();
        if (!info) {
            (void)act::message("gpu-ui: {}", info.error().message());
            return;
        }
        if (!info->available || !info->sceneCopy)
            return;

        auto pack = gpu::load_pack(std::span<const uint8_t>(gpu_ui_pack, sizeof(gpu_ui_pack)));
        if (!pack) {
            (void)act::message("gpu-ui: the shader pack was rejected ({}). It is probably the empty placeholder: build the plugin with CONDUIT_DIR and SLANGC set.", pack.error().message());
            return;
        }
        pack_ = *pack;
        ui_ = pack_.program("ui");
        blur_ = pack_.program("blur");
        if (!ui_ || !blur_) {
            (void)act::message("gpu-ui: the shader pack has no ui/blur program for {}", info->shaderTarget);
            return;
        }

        auto sampler = gpu::linear_sampler();
        auto uiPipe = gpu::create_pipeline(gpu::quad_pipeline(pack_, "ui"));
        auto blurPipe = gpu::create_pipeline(gpu::quad_pipeline(pack_, "blur", false));
        auto scene = gpu::create_screen_texture(gpu::GpuFormat::Screen, 1.0f);
        auto ping = gpu::create_screen_texture(gpu::GpuFormat::Screen, kScale);
        auto pong = gpu::create_screen_texture(gpu::GpuFormat::Screen, kScale);
        constexpr uint64_t kVertexBytes = 4096 * sizeof(gpu::Vertex2D);
        auto vertices = gpu::create_buffer(kVertexBytes, gpu::buffer_usage({gpu::GpuBufferUsage::Vertex, gpu::GpuBufferUsage::CopyDst}));
        auto indices = gpu::create_buffer(4096 * 6 * sizeof(uint32_t), gpu::buffer_usage({gpu::GpuBufferUsage::Index, gpu::GpuBufferUsage::CopyDst}));
        if (!sampler || !uiPipe || !blurPipe || !scene || !ping || !pong || !vertices || !indices) {
            (void)act::message("gpu-ui: creating resources failed");
            return;
        }
        sampler_ = *sampler;
        uiPipe_ = *uiPipe;
        blurPipe_ = *blurPipe;
        scene_ = *scene;
        ping_ = *ping;
        pong_ = *pong;
        vertices_ = *vertices;
        indices_ = *indices;
        ready_ = true;
        failed_ = false;
        width_ = height_ = 0;
        layout(info->width, info->height);
    }

    void layout(uint32_t width, uint32_t height) {
        width_ = width;
        height_ = height;
        const float w = static_cast<float>(width), h = static_cast<float>(height);

        // Geometry: quad 0 is the full-screen rectangle for the blur passes, quad 1 the glass panel,
        // quad 2 an accent bar. Everything is in pixels of the screen.
        const float panelX0 = w * 0.05f, panelY0 = h * 0.2f, panelX1 = w * 0.25f, panelY1 = h * 0.6f;
        gpu::QuadBatch batch;
        batch.quad(0, 0, w * kScale, h * kScale, 0, 0, 1, 1, 0xFFFFFFFFu);
        batch.rect(panelX0, panelY0, panelX1, panelY1, 0x66FFFFFFu);
        batch.rect(panelX0, panelY0, panelX1, panelY0 + 6, 0xFFE0A030u);
        (void)gpu::write_buffer(vertices_, 0, batch.vertices());
        (void)gpu::write_buffer(indices_, 0, batch.indices());

        // Layer 1, under the HUD: copy the scene, blur it in two passes, draw the glass panel over it.
        {
            gpu::Uniforms blurH(*blur_), blurV(*blur_), ui(*ui_);
            blurH.set("u_screen", std::array<float, 4>{w * kScale, h * kScale, 0, 0});
            blurV.set("u_screen", std::array<float, 4>{w * kScale, h * kScale, 0, 0});
            blurH.set("u_blur", std::array<float, 4>{1.0f / (w * kScale), 0, 1.5f, 0});
            blurV.set("u_blur", std::array<float, 4>{0, 1.0f / (h * kScale), 1.5f, 0});
            ui.set("u_screen", std::array<float, 4>{w, h, 0, 0});
            ui.set("u_mode", std::array<float, 4>{2, 14, 0, 0});
            ui.set("u_box", std::array<float, 4>{(panelX0 + panelX1) * 0.5f, (panelY0 + panelY1) * 0.5f,
                                                 (panelX1 - panelX0) * 0.5f, (panelY1 - panelY0) * 0.5f});

            gpu::Commands c;
            c.push_label("gpu-ui blur");
            c.copy_screen(scene_);

            c.begin_pass(ping_, false);
            c.pipeline(blurPipe_).uniforms(blurH).texture(0, scene_, sampler_);
            c.vertex_buffer(0, vertices_).index_buffer(indices_, true).draw_indexed(6, 1, 0, 0);
            c.end_pass();

            c.begin_pass(pong_, false);
            c.pipeline(blurPipe_).uniforms(blurV).texture(0, ping_, sampler_);
            c.vertex_buffer(0, vertices_).index_buffer(indices_, true).draw_indexed(6, 1, 0, 0);
            c.end_pass();

            c.begin_pass_screen();
            c.pipeline(uiPipe_).uniforms(ui).texture(1, pong_, sampler_);
            c.vertex_buffer(0, vertices_).index_buffer(indices_, true).draw_indexed(6, 1, 6, 0);
            c.end_pass();
            c.pop_label();
            (void)c.submit("glass", gpu::GpuStage::UnderHud, 0);
        }

        // Layer 2, over everything: a flat accent bar with square corners. Slots that are not bound read a white texture.
        {
            gpu::Uniforms ui(*ui_);
            ui.set("u_screen", std::array<float, 4>{w, h, 0, 0});
            ui.set("u_mode", std::array<float, 4>{0, 0, 0, 0});
            gpu::Commands c;
            c.begin_pass_screen();
            c.pipeline(uiPipe_).uniforms(ui);
            c.vertex_buffer(0, vertices_).index_buffer(indices_, true).draw_indexed(6, 1, 12, 0);
            c.end_pass();
            (void)c.submit("accent", gpu::GpuStage::Overlay, 0);
        }
    }

    gpu::Pack pack_;
    const gpu::GpuProgramInfo* ui_ = nullptr;
    const gpu::GpuProgramInfo* blur_ = nullptr;
    uint32_t sampler_ = 0, uiPipe_ = 0, blurPipe_ = 0, scene_ = 0, ping_ = 0, pong_ = 0, vertices_ = 0, indices_ = 0;
    uint32_t width_ = 0, height_ = 0;
    bool ready_ = false;
    bool failed_ = false;
};
PERLIN_REGISTER(GpuUi)
