#include "mod/MyMod.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <dlfcn.h>
#include <filesystem>
#include <izer// 注意：不 #include "pl/cpp/Hook.hpp"，避免链接 pl_hook

#define SYM_ACTOR_GETPOS  "?getPosition@Actor@@UEA?AVVec3@@XZ"
#define SYM_EGL_SWAP      "eglSwapBuffers"
#define SYM_PL_HOOK       "pl_hook"
#define SYM_PL_UNHOOK     "pl_unhook"

namespace {

void *findSymbol(const char *name, void *handle = nullptr) {
    return dlsym(handle ? handle : RTLD_DEFAULT, name);
}

void *findSymbolAny(std::initializer_list<const char *> names) {
    for (auto name : names) {
        void *addr = dlsym(RTLD_DEFAULT, name);
        if (addr) return addr;
    }
    return nullptr;
}

// pl_hook 的函数签名（来自 pl/c/Hook.h）
// int pl_hook(void *target, void *detour, void **original, int priority);
using PlHookFn   = int  (*)(void *, void *, void **, int);
using PlUnhookFn = int  (*)(void *, void *);

// 运行时解析到的 hook 函数指针
PlHookFn   g_plHook   = nullptr;
PlUnhookFn g_plUnhook = nullptr;

} // anonymous namespace

namespace my_mod {

MyMod &MyMod::getInstance() {
    static MyMod instance;
    return instance;
}

pl::mod::NativeMod &MyMod::getSelf() const {
    return *pl::mod::NativeMod::current();
}

bool MyMod::load() {
    auto &s = getSelf();
    s.getLogger().debug("[MiniMap] load()");

    std::error_code ec;
    std::filesystem::create_directories(s.getDataDir(), ec);
    std::filesystem::create_directories(s.getConfigDir(), ec);

    pl::config::ConfigFile<ModConfig> cf;
    if (!cf.load()) {
        s.getLogger().warn("[MiniMap] config load failed, using defaults");
        config = ModConfig{};
    } else {
        config = cf.value();
    }

    minimapOpen = false;
    s.getLogger().info("[MiniMap] load() ok, waypoints={}", config.waypoints.size());
    return true;
}

bool MyMod::enable() {
    auto &s = getSelf();

    if (!config.enabled) {
        s.getLogger().info("[MiniMap] disabled by config");
        return true;
    }

    // 运行时解析 pl_hook / pl_unhook（由 preloader 提供）
    g_plHook   = reinterpret_cast<PlHookFn>(findSymbol(SYM_PL_HOOK));
    g_plUnhook = reinterpret_cast<PlUnhookFn>(findSymbol(SYM_PL_UNHOOK));
    if (!g_plHook || !g_plUnhook) {
        s.getLogger().warn("[MiniMap] pl_hook/pl_unhook NOT FOUND — hooks disabled");
    } else {
        s.getLogger().info("[MiniMap] pl_hook @ {}, pl_unhook @ {}",
                           (void *)g_plHook, (void *)g_plUnhook);
    }

    void *addrGetPos = findSymbolAny({
        SYM_ACTOR_GETPOS,
        "_ZN5Actor11getPositionEv",
        "_ZN5Actor6getPosEv"
    });
    if (addrGetPos) {
        s.getLogger().info("[MiniMap] found Actor::getPosition @ {}", addrGetPos);
    } else {
        s.getLogger().warn("[MiniMap] Actor::getPosition NOT FOUND");
    }

    void *addrSwap = findSymbol(SYM_EGL_SWAP);
    if (addrSwap) {
        s.getLogger().info("[MiniMap] found eglSwapBuffers @ {}", addrSwap);
    } else {
        s.getLogger().error("[MiniMap] eglSwapBuffers NOT FOUND");
        return false;
    }

    if (!installHooks()) {
        s.getLogger().error("[MiniMap] hook install failed");
        return false;
    }

    s.getLogger().info("[MiniMap] enabled. Tap top-right button.");
    return true;
}

bool MyMod::disable() {
    getSelf().getLogger().debug("[MiniMap] disable()");
    {
        std::lock_guard<std::mutex> lk(mtx);
        minimapOpen = false;
    }
    uninstallHooks();
    return true;
}

bool MyMod::unload() {
    getSelf().getLogger().debug("[MiniMap] unload()");
    uninstallHooks();
    return true;
}

bool MyMod::installHooks() {
    auto &s = getSelf();
    if (!g_plHook) {
        s.getLogger().warn("[MiniMap] skip hooks — pl_hook not available");
        return true;
    }

    // ① 玩家位置 Hook
    void *addrGetPos = findSymbolAny({
        SYM_ACTOR_GETPOS, "_ZN5Actor11getPositionEv", "_ZN5Actor6getPosEv"
    });
    if (addrGetPos) {
        int rc = g_plHook(
            addrGetPos,
            reinterpret_cast<void *>(
                +[](void *actor) -> Vec3 {
                    auto &mod = MyMod::getInstance();
                    using Fn = Vec3 (*)(void *);
                    auto orig = reinterpret_cast<Fn>(mod.origPos);
                    Vec3 result = orig(actor);
                    mod.onPlayerPos(result.x, result.y, result.z, 0.0f);
                    return result;
                }),
            &origPos,
            2  // pl::hook::PriorityNormal = 2
        );
        if (rc == 0) s.getLogger().info("[MiniMap] pos hook ok");
        else s.getLogger().error("[MiniMap] pos hook failed: {}", rc);
    }

    // ② 渲染 Hook
    void *addrSwap = findSymbol(SYM_EGL_SWAP);
    if (addrSwap) {
        int rc = g_plHook(
            addrSwap,
            reinterpret_cast<void *>(
                +[](void *dpy, void *surf) -> int {
                    auto &mod = MyMod::getInstance();
                    using Fn = int (*)(void *, void *);
                    auto orig = reinterpret_cast<Fn>(mod.origRender);
                    int ret = orig(dpy, surf);
                    mod.drawUI();
                    return ret;
                }),
            &origRender,
            2
        );
        if (rc == 0) s.getLogger().info("[MiniMap] render hook ok");
    }

    return true;
}

bool MyMod::uninstallHooks() {
    if (!g_plUnhook) return true;

    if (origPos) {
        void *addr = findSymbolAny({SYM_ACTOR_GETPOS, "_ZN5Actor11getPositionEv"});
        if (addr) g_plUnhook(addr, hookPos);
    }
    if (origRender) {
        void *_       Un hookRender = hookTouch = nullptr;
    origPos = origRender = origTouch = nullptr;
    return true;
}

void MyMod::onPlayerPos(float x, float y, float z, float yaw) {
    std::lock_guard<std::mutex> lk(mtx);
    playerPos = {x, y, z};
    playerYaw = yaw;
}

void MyMod::onScreenSize(int w, int h) {
    std::lock_guard<std::mutex> lk(mtx);
    screenW = w;
    screenH = h;
}

void MyMod::onTouch(float sx, float sy, bool down) {
    if (!down) return;
    std::lock_guard<std::mutex> lk(mtx);

    int W = screenW, H = screenH;
    if (W == 0 || H == 0) return;

    float btnCx = config.btnX * W;
    float btnCy = config.btnY * H;
    float btnR  = config.btnSize * 0.5f;
    if (inCircle(sx, sy, btnCx, btnCy, btnR)) {
        minimapOpen = !minimapOpen;
        getSelf().getLogger().info("[MiniMap] {}", minimapOpen ? "OPEN" : "CLOSED");
        return;
    }

    if (!minimapOpen) return;

    float mcX = W - config.mapSize * 0.5f - 20.0f;
    float mcY = config.mapSize * 0.5f + 60.0f;
    float mR  = config.mapSize * 0.5f;
    if (!inCircle(sx, sy, mcX, mcY, mR)) return;

    float wx, wz;
    screenToWorld(sx, sy, wx, wz);

    for (const auto &wp : config.waypoints) {
        float dx = wp.x - playerPos.x;
        float dz = wp.z - playerPos.z;
        if (config.rotateWithPlayer) {
            float rad = playerYaw * (M_PI / 180.0f);
            float rx = dx * std::cos(rad) - dz * std::sin(rad);
            float ry = dx * std::sin(rad) + dz * std::cos(rad);
            dx = rx; dz = ry;
        }
        float psx = mcX + dx / config.mapScale;
        float psy = mcY - dz / config.mapScale;
        if (std::hypot(sx - psx, sy - psy) < 18.0f) {
            getSelf().getLogger().info("[MiniMap] teleport -> {}", wp.name);
            teleportToWaypoint(wp);
            return;
        }
    }

    char buf[64];
    std::snprintf(buf, sizeof(buf), "WP_%.0f_%.0f_%.0f", wx, playerPos.y, wz);
    addWaypoint(std::string(buf), wx, playerPos.y, wz);
}

void MyMod::screenToWorld(float sx, float sy, float &wx, float &wz) {
    int W = screenW, H = screenH;
    float mcX = W - config.mapSize * 0.5f - 20.0f;
    float mcY = config.mapSize * 0.5f + 60.0f;
    float dx = (sx - mcX) * config.mapScale;
    float dz = (mcY - sy) * config.mapScale;

    if (config.rotateWithPlayer) {
        float rad = -playerYaw * (M_PI / 180.0f);
        wx = playerPos.x + dx * std::cos(rad) - dz * std::sin(rad);
        wz = playerPos.z + dx * std::sin(rad) + dz * std::cos(rad);
    } else {
        wx = playerPos.x + dx;
        wz = playerPos.z + dz;
    }
}

void MyMod::drawUI() {
    std::lock_guard<std::mutex> lk(mtx);
    int W = screenW, H = screenH;
    if (W == 0 || H == 0) return;

    float bx = config.btnX * W;
    float by = config.btnY * H;
    float br = config.btnSize * 0.5f;
    uint8_t ba = (uint8_t)(config.btnAlpha * 255);
    drawFillCircle(bx, by, br, 50, 50, 50, ba);
    drawCircle(bx, by, br, 180, 180, 180, ba);
    float sq = br * 0.35f;
    if (minimapOpen) {
        drawFillCircle(bx - sq, by - sq, br * 0.12f, 255, 60, 60, 200);
        drawFillCircle(bx + sq, by - sq, br * 0.12f, 255, 60, 60, 200);
        drawFillCircle(bx - sq, by + sq, br * 0.12f, 255, 60, 60, 200);
        drawFillCircle(bx + sq, by + sq, br * 0.12f, 255, 60, 60, 200);
    } else {
        drawFillCircle(bx, by, br * 0.2f, 100, 200, 255, 200);
    }

    if (!minimapOpen) return;

    float mx = W - config.mapSize * 0.5f - 20.0f;
    float my = config.mapSize * 0.5f + 60.0f;
    float mr = config.mapSize * 0.5f;
    uint8_t ma = (uint8_t)(config.mapAlpha * 255);

    drawFillCircle(mx, my, mr, 10, 10, 20, ma);
    drawCircle(mx, my, mr, 200, 200, 200, ma);

    float dlen = mr * 0.85f;
    float baseA = config.rotateWithPlayer ? (playerYaw * M_PI / 180.0f) : 0.0f;
    for (int i = 0; i < 4; ++i) {
        float a = baseA + i * M_PI / 2.0f;
        drawFillCircle(mx + std::sin(a) * dlen, my - std::cos(a) * dlen, 3.0f, 180, 180, 180, 180);
    }
    drawFillCircle(mx + std::sin(baseA) * dlen, my - std::cos(baseA) * dlen, 5.0f, 255, 50, 50, 220);

    float angle = config.rotateWithPlayer ? 0.0f : (playerYaw * M_PI / 180.0f);
    drawFillCircle(mx + std::sin(angle) * 8.0f, my - std::cos(angle) * 8.0f, 4.5f, 255, 255, 255, 240);

    for (const auto &wp : config.waypoints) {
        float dx = wp.x - playerPos.x;
        float dz = wp.z - playerPos.z;
        if (config.rotateWithPlayer) {
            float rad = playerYaw * (M_PI / 180.0f);
            float rx = dx * std::cos(rad) - dz * std::sin(rad);
            float ry = dx * std::sin(rad) + dz * std::cos(rad);
            dx = rx; dz = ry;
        }
        float sx = mx + dx / config.mapScale;
        float sy = my - dz / config.mapScale;
        float dist = std::hypot(sx - mx, sy - my);
        if (dist > mr - 6.0f) {
            float ea = std::atan2(sy - my, sx - mx);
            sx = mx + std::cos(ea) * (mr - 6.0f);
            sy = my + std::sin(ea) * (mr - 6.0f);
        }
        uint8_t cr = 255, cg = 80, cb = 80;
        hexToRGB(wp.color, cr, cg, cb);
        drawFillCircle(sx, sy, 6.0f, cr, cg, cb, 140);
        drawFillCircle(sx, sy, 3.0f, cr, cg, cb, 230);
    }
}

void MyMod::addWaypoint(const std::string &name, float x, float y, float z) {
    std::lock_guard<std::mutex> lk(mtx);
    config.waypoints.push_back({name, x, y, z, 0, "#FF4444"});
    saveCfg();
}

void MyMod::removeWaypoint(size_t i) {
    std::lock_guard<std::mutex> lk(mtx);
    if (i < config.waypoints.size()) {
        config.waypoints.erase(config.waypoints.begin() + i);
        saveCfg();
    }
}

void MyMod::teleportToWaypoint(const Waypoint &wp) {
    getSelf().getLogger().info(
        "[MiniMap] TELEPORT to {} ({:.0f}, {:.0f}, {:.0f}) [need command hook]",
        wp.name, wp.x, wp.y, wp.z);
}

void MyMod::saveCfg() {
    pl::config::ConfigFile<ModConfig> cf(config);
    cf.save();
}

bool MyMod::inCircle(float px, float py, float cx, float cy, float r) {
    return (px - cx) * (px - cx) + (py - cy) * (py - cy) <= r * r;
}

bool MyMod::hexToRGB(const std::string &h, uint8_t &R, uint8_t &G, uint8_t &B) {
    if (h.size() != 7 || h[0] != '#') return false;
    unsigned int v;
    std::stringstream ss;
    ss << std::hex << h.substr(1);
    ss >> v;
    if (ss.fail()) return false;
    R = (v >> 16) & 0xFF;
    G = (v >> 8) & 0xFF;
    B = v & 0xFF;
    return true;
}

void MyMod::drawCircle(float, float, float, uint8_t, uint8_t, uint8_t, uint8_t) {}
void MyMod::drawFillCircle(float, float, float, uint8_t, uint8_t, uint8_t, uint8_t) {}

} // namespace my_mod
