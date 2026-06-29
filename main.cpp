#include "AmbientTools.h"
#include "stub.h"
#include <android/log.h>
#include <atomic>

#define TAG "VirtualMouse"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

namespace VirtualMouse {
    std::atomic<bool> visible{false};
}

// ── RenderAPI callback ────────────────────────────────────────
static void OnRender() {
    if (!At_IsBound()) return;

    // Init cursor to center if first show
    bool inMenu = At_SessionInMenu();
    bool wasVisible = VirtualMouse::visible.load();

    if (inMenu && !wasVisible) {
        // setActiveFromMenu(true) VirtualMouse
        VirtualMouse::visible.store(true);
        LOGD("Cursor shown");
    } else if (!inMenu && wasVisible) {
        // setActiveFromMenu(false)
        VirtualMouse::visible.store(false);
        LOGD("Cursor hidden");
    }
}

// ── Entry point ───────────────────────────────────────────────
__attribute__((constructor))
void init() {
    LOGD("VirtualMouse mod init");
    RenderAPI::Register(OnRender);
}
LOGD("SPAM SPAM SPAM SPAM SPAM SPAM SPAM SPAM");