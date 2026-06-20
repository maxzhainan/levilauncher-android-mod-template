#include "mod/MyMod.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <dlfcn.h>
#include <filesystem>
#include <initializer_list>
#include <sstream>

#include "pl/cpp/Config.hpp"
#include "pl/cpp/Hook.hpp"
#include "pl/cpp/Mod.hpp"

// ============================================================================
// dlsym 符号名 — 自动查找，无需手动找偏移量
// ============================================================================
#define SYM_ACTOR_GETPOS  "?getPosition@Actor@@UEA?AVVec3@@XZ"
#define SYM_EGL_SWAP      "eglSwapBuffers"

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

} // anonymous namespace

namespace my_mod {

// ============================================================================
// 单例
// ============================================================================
MyMod &MyMod::getInstance() {
    static MyMod instance;
    return instance;
}

pl::mod::NativeMod &MyMod::getSelf() const {
    return *pl::mod::NativeMod::current();
}

// ============================================================================
// load()
// ============================================================================
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

    config.$minimapOpen = false;
    s.getLogger().info("[MiniMap] load() ok, waypoints={}", config.waypoints.size());
    return true;
}

// ============================================================================
// enable()
// ============================================================================
bool MyMod::enable() {
    auto &s = getSelf();

    if (!config.enabled) {
        s.getLogger().info("[MiniMap] disabled by config");
        return true;
    }

    // 查找符号
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

// ============================================================================
// disable()
// ============================================================================
bool MyMod::disable() {
    getSelf().getLogger().debug("[MiniMap] disable()");
    {
        std::lock_guard<std::mutex> lk(mtx);
        config.$minimapOpen = false;
    }
    uninstallHooks();
    return true;
}

// ============================================================================
// unload()
// ============================================================================
bool MyMod::unload() {
    getSelf().getLogger().debug("[MiniMap] unload()");
    uninstallHooks();
    return true;
}

// ============================================================================
// 安装 Hook
// ============================================================================
bool MyMod::installHooks() {
    auto &s = getSelf();

    // ① 玩家位置
    void *addrGetPos = findSymbolAny({
        SYM_ACTOR_GETPOS, "_ZN5Actor11getPositionEv", "_ZN5Actor6getPosEv"
    });
    if (addrGetPos) {
        int rc = pl::hook::hook(
            reinterpret_cast<pl::hook::FuncPtr>(addrGetPos),
            reinterpret_cast<pl::hook::FuncPtr>(
                +[](void *actor) -> Vec3 {
                    auto &mod = MyMod::getInstance();
                    using Fn = Vec3 (*)(void *);
                    auto orig = reinterpret_cast<Fn>(mod.origPos);
                    Vec3 result = orig(actor);
                    mod.onPlayerPos(result.x, result.y, result.z, 0.0f);
                    return result;
                }),
            reinterpret_cast<pl::hook::FuncPtr *>(&origPos),
            pl::hook::PriorityNormal);
        if (rc == 0) s.getLogger().info("[MiniMap] pos hook ok");
        else s.getLogger().error("[MiniMap] pos hook failed: {}", rc);
    }

    // ② 渲染
    void *addrSwap = findSymbol(SYM_EGL_SWAP);
    if (addrSwap) {
        int rc = pl::hook::hook(
            reinterpret_cast<pl::hook::FuncPtr>(addrSwap),
            reinterpret_cast<pl::hook::FuncPtr>(
                +[](void *dpy, void *surf) -> int {
                    auto &mod = MyMod::getInstance();
                    using Fn = int (*)(void *, void *);
                    auto orig = reinterpret_cast<Fn>(mod.origRender);
                    int ret = orig(dpy, surf);
                    mod.drawUI();
                    return ret;
                }),
            reinterpret_cast<pl::hook::FuncPtr *>(&origRender),
            pl::hook::PriorityNormal);
        if (rc == 0) s.getLogger().info("[MiniMap] render hook ok");
    }

    return true;
}

// ============================================================================
// 卸载 Hook
// ============================================================================
bool MyMod::uninstallHooks() {
    if (origPos && hookPos) {
        void *addr = findSymbolAny({SYM_ACTOR_GETPOS, "_ZN5Actor11getPositionEv"});
        if (addr) pl::hook::unhook(
            reinterpret_cast<pl::hook::FuncPtr>(addr),
            reinterpret_cast<pl::hook::FuncPtr>(hookPos));
    }
    if (origRender && hookRender) {
        void *addr = findSymbol(SYM_EGL_SWAP);
        if (addr) pl::hook::unhook(
            reinterpret_cast<pl::hook::FuncPtr>(addr),
            reinterpret_cast<pl::hook::FuncPtr>(hookRender));
    }
    hookPos = hookRender = hookTouch = nullptr;
    origPos = origRender = origTouch = nullptr;
    return true;
}

// ============================================================================
// 回调
// ============================================================================
void MyMod::onPlayerPos(float x, float y, float z, float yaw) {
    std::lock_guard<std::mutex> lk(mtx);
    playerPos = {x, y, z};
    playerYaw = yaw;
}

void MyMod::onScreenSize(int w, int h) {
    std::lock_guard<std::mutex> lk(mtx);
    config.$screenW = w;
    config.$screenH = h;
}

// ============================================================================
// 触摸
// ============================================================================
void MyMod::onTouch(float sx, float sy, bool down) {
    if (!down) return;
    std::lock_guard<std::mutex> lk(mtx);

    int W = config.$screenW, H = config.$screenH;
    if (W == 0 || H == 0) return;

    float btnCx = config.btnX * W;
    float btnCy = config.btnY * H;
    float btnR  = config.btnSize * 0.5f;
    if (inCircle(sx, sy, btnCx, btnCy, btnR)) {
        config.$minimapOpen = !config.$minimapOpen;
        getSelf().getLogger().info("[MiniMap] {}", config.$minimapOpen ? "OPEN" : "CLOSED");
        return;
    }

    if (!config.$minimapOpen) return;

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

// ============================================================================
// 屏幕→世界
// ============================================================================
void MyMod::screenToWorld(float sx, float sy, float &wx, float &wz) {
    int W = config.$screenW, H = config.$screenH;
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

// ============================================================================
// 绘制
// ============================================================================
void MyMod::drawUI() {
    std::lock_guard<std::mutex> lk(mtx);
    int W = config.$screenW, H = config.$screenH;
    if (W == 0 || H == 0) return;

    float bx = config.btnX * W;
    float by = config.btnY * H;
    float br = config.btnSize * 0.5f;
    uint8_t ba = (uint8_t)(config.btnAlpha * 255);
    drawFillCircle(bx, by, br, 50, 50, 50, ba);
    drawCircle(bx, by, br, 180, 180, 180, ba);
    float sq = br * 0.35f;
    if (config.$minimapOpen) {
        drawFillCircle(bx - sq, by - sq, br * 0.12f, 255, 60, 60, 200);
        drawFillCircle(bx + sq, by - sq, br * 0.12f, 255, 60, 60, 200);
        drawFillCircle(bx - sq, by + sq, br * 0.12f, 255, 60, 60, 200);
        drawFillCircle(bx + sq, by + sq, br * 0.12f, 255, 60, 60, 200);
    } else {
        drawFillCircle(bx, by, br * 0.2f, 100, 200, 255, 200);
    }

    if (!config.$minimapOpen) return;

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

// ============================================================================
// 传送点管理
// ============================================================================
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

// ============================================================================
// 保存配置
// ============================================================================
void MyMod::saveCfg() {
    ModConfig save = config;
    save.$minimapOpen = false;
    save.$screenW = 1080;
    save.$screenH = 1920;
    pl::config::ConfigFile<ModConfig> cf(save);
    cf.save();
}

// ============================================================================
// 小工具
// ============================================================================
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

// 绘制占位 — 需替换为真实 GLES 调用
void MyMod::drawCircle(float, float, float, uint8_t, uint8_t, uint8_t, uint8_t) {}
void MyMod::drawFillCircle(float, float, float, uint8_t, uint8_t, uint8_t, uint8_t) {}

} // namespace my_mod
