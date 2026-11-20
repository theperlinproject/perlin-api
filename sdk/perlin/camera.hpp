#pragma once
#include "perlin/gen/calls.hpp"
#include "perlin/rpc.hpp"

namespace perlin::camera {
    using abi::CameraDetachState;
    using abi::CameraMode;

    inline Result<void> detach(const CameraDetachState& state) {
        return api::camera_detach(state);
    }

    inline Result<void> attach() {
        return api::camera_detach(CameraDetachState{});
    }

    inline Result<CameraDetachState> detached() {
        return api::camera_detach_get();
    }

    struct FreelookOptions {
        float distance = 4.0f;
        bool collide = true;
        bool freezeMovement = false;
        float fov = 0.0f;
        int ticks = 2;
    };

    inline Result<void> freelook(const FreelookOptions& o = {}) {
        CameraDetachState s;
        s.mode = CameraMode::Freelook;
        s.distance = o.distance;
        s.collide = o.collide;
        s.freezeMovement = o.freezeMovement;
        s.fov = o.fov;
        s.ticks = o.ticks;
        return detach(s);
    }

    struct FreecamOptions {
        float speed = 10.0f;
        bool showSelf = true;
        float fov = 0.0f;
        int ticks = 2;
    };

    inline Result<void> freecam(const FreecamOptions& o = {}) {
        CameraDetachState s;
        s.mode = CameraMode::Free;
        s.speed = o.speed;
        s.showSelf = o.showSelf;
        s.freezeMovement = true;
        s.fov = o.fov;
        s.ticks = o.ticks;
        return detach(s);
    }

    inline Result<void> fly_to(double x, double y, double z, float yaw, float pitch, float fov = 0.0f, int ticks = 2) {
        CameraDetachState s;
        s.mode = CameraMode::Free;
        s.x = x;
        s.y = y;
        s.z = z;
        s.yaw = yaw;
        s.pitch = pitch;
        s.fov = fov;
        s.mouseLook = false;
        s.keyFly = false;
        s.freezeMovement = true;
        s.ticks = ticks;
        return detach(s);
    }

    class Toggle {
    public:
        explicit Toggle(CameraDetachState state) : state_(state) {}

        void hold(bool down) { on_ = down; }
        void toggle() { on_ = !on_; }
        void set(bool on) { on_ = on; }
        bool on() const { return on_; }
        CameraDetachState& state() { return state_; }

        void tick() {
            if (on_) {
                state_.ticks = 3;
                (void)detach(state_);
                was_ = true;
            } else if (was_) {
                (void)attach();
                was_ = false;
            }
        }

    private:
        CameraDetachState state_;
        bool on_ = false;
        bool was_ = false;
    };

    inline Toggle freelook_toggle(float distance = 4.0f) {
        CameraDetachState s;
        s.mode = CameraMode::Freelook;
        s.distance = distance;
        return Toggle(s);
    }

    inline Toggle freecam_toggle(float speed = 10.0f) {
        CameraDetachState s;
        s.mode = CameraMode::Free;
        s.speed = speed;
        s.freezeMovement = true;
        return Toggle(s);
    }
}
