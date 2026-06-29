#include "cursor.h"
#include "AmbientTools.h"
#include "stub.h"
#include "imgui.h"
#include <android/log.h>
#include <atomic>

#define TAG "VirtualMouse"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

namespace VirtualMouse {
    std::atomic<bool> visible{false};
    float cursorX = 0.0f;
    float cursorY = 0.0f;
    float screenW = 720.0f;
    float screenH = 1600.0f;
}

// Forward declare render fn from cursor_render.cpp
namespace VirtualMouse { void render(); }

// ── RenderAPI callback ────────────────────────────────────────
static void OnRender() {
    if (!At_IsBound()) return;

    ImGuiIO& io = ImGui::GetIO();
    VirtualMouse::screenW = io.DisplaySize.x;
    VirtualMouse::screenH = io.DisplaySize.y;

    // Init cursor to center if first show
    bool inMenu = At_SessionInMenu();
    bool wasVisible = VirtualMouse::visible.load();

    if (inMenu && !wasVisible) {
        if (VirtualMouse::cursorX == 0.0f && VirtualMouse::cursorY == 0.0f) {
            VirtualMouse::cursorX = VirtualMouse::screenW * 0.5f;
            VirtualMouse::cursorY = VirtualMouse::screenH * 0.5f;
        }
        VirtualMouse::visible.store(true);
        LOGD("Cursor shown");
    } else if (!inMenu && wasVisible) {
        VirtualMouse::visible.store(false);
        LOGD("Cursor hidden");
    }

    VirtualMouse::render();
}

// ── TouchAPI callback ─────────────────────────────────────────
static bool OnTouch(const TouchEvent* ev) {
    if (!VirtualMouse::visible.load()) return false;

    switch (ev->action) {
        case 2: { // ACTION_MOVE
            // Use raw delta — no history like Java side, so track last pos
            static float lastX = -1.0f;
            static float lastY = -1.0f;

            if (lastX >= 0.0f) {
                float dx = ev->x - lastX;
                float dy = ev->y - lastY;
                VirtualMouse::cursorX += dx * 3;
                VirtualMouse::cursorY += dy * 3;

                // Clamp to screen
                VirtualMouse::cursorX = std::max(0.0f, std::min(VirtualMouse::cursorX, VirtualMouse::screenW));
                VirtualMouse::cursorY = std::max(0.0f, std::min(VirtualMouse::cursorY, VirtualMouse::screenH));
            }
            lastX = ev->x;
            lastY = ev->y;
            return true; // consume move so game doesn't rotate camera
        }
        case 0: // ACTION_DOWN
            lastX = lastY = -1.0f;
            return false; // let clicks through to UI
        case 1: // ACTION_UP
            return false;
        default:
            return false;
    }
}

// ── Entry point ───────────────────────────────────────────────
__attribute__((constructor))
void init() {
    LOGD("VirtualMouse mod init");
    RenderAPI::Register(OnRender);
    TouchAPI::RegisterCallback(OnTouch);
}
