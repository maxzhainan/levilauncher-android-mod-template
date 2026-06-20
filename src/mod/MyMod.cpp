#include "mod/MyMod.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <sstream>

#include "pl/cpp/Config.hpp"
#include "pl/cpp/Hook.hpp"
#include "pl/cpp/Mod.hpp"

// ============================================================================
// 以下为 OpenGL ES 2.0 函数指针（通过 dlsym 动态获取）
// 因为模板环境不要求链接 GLES，运行时动态解析即可。
// 需要替换为真实函数签名 — 此处仅作占位说明。
// ============================================================================
namespace gles {
// 类型占位：实际应为 PFNGLxxxPROC
using GLFunc = void (*)();  // 所有 GL 函数统一占位为 void(*)()

// 占位函数指针（在 enable() 阶段通过 eglGetProcAddress / dlsym 填充）
static GLFunc glUseProgram   = nullptr;
static GLFunc glUniform4f    = nullptr;
static GLFunc glUniformMatrix4fv = nullptr;
static GLFunc glBindBuffer   = nullptr;
static GLFunc glBufferData   = nullptr;
static GLFunc glEnableVertexAttribArray = nullptr;
static GLFunc glVertexAttribPointer     = nullptr;
static GLFunc glDrawArrays   = nullptr;
static GLFunc glEnable       = nullptr;
static GLFunc glDisable      = nullptr;
static GLFunc glBlendFunc    = nullptr;
static GLFunc glLineWidth    = nullptr;
} // namespace gles

namespace my_mod {

// ============================================================================
// 单例 & 辅助
// ============================================================================
MyMod &MyMod::getInstance() {
    static MyMod instance;
    return instance;
}

pl::mod::NativeMod &MyMod::getSelf() const {
    return *pl::mod::NativeMod::current();
}

// ============================================================================
// load() — 加载配置 & 初始化目录
// ============================================================================
bool MyMod::load() {
    auto &self = getSelf();
    self.getLogger().debug("[MiniMap::load] Initializing...");

    // 1. 创建 data / config 目录
    std::error_code ec;
    std::filesystem::create_directories(self.getDataDir(), ec);
    if (ec) {
        self.getLogger().error("[MiniMap::load] data dir: {}", ec.message());
        return false;
    }
    std::filesystem::create_directories(self.getConfigDir(), ec);
    if (ec) {
        self.getLogger().error("[MiniMap::load] config dir: {}", ec.message());
        return false;
    }

    // 2. 加载 typed config（自动合并/升级/写回）
    pl::config::ConfigFile<MinimapConfig> configFile;
    if (!configFile.load()) {
        self.getLogger().warn("[MiniMap::load] Config load failed, using defaults.");
        config = MinimapConfig{};
    } else {
        config = configFile.value();
    }

    // 运行时状态初始化
    config.$minimapOpen = false;
    config.$screenW = 1080;
    config.$screenH = 1920;

    self.getLogger().info(
        "[MiniMap::load] Loaded. waypoints={}, mapSize={:.0f}px, scale={:.1f}",
        config.waypoints.size(), config.mapSize, config.mapScale);

    return true;
}

// ============================================================================
// enable() — 安装 Hook
// ============================================================================
bool MyMod::enable() {
    auto &self = getSelf();

    if (!config.enabled) {
        self.getLogger().info("[MiniMap::enable] Disabled by config.");
        return true;
    }

    self.getLogger().debug("[MiniMap::enable] Installing hooks...");

    // ---------------------------------------------------------------
    // 占位 Hook 1：玩家位置
    // 目标函数：?getPosition@Actor@@UEA?AVVec3@@XZ
    // 或偏移量 0x02A1B3C0（以实际版本为准）
    //
    // 安装示例：
    //   void *addr = /* 通过 dlsym 或基址+偏移获取 */;
    //   pl::hook::hook(
    //       reinterpret_cast<pl::hook::FuncPtr>(addr),
    //       reinterpret_cast<pl::hook::FuncPtr>(+[](void *actor) -> Vec3 {
    //           auto &mod = MyMod::getInstance();
    //           // 调用原始函数
    //           Vec3 pos = ...;
    //           mod.onPlayerPositionUpdated(pos.x, pos.y, pos.z, ...);
    //           return pos;
    //       }),
    //       &mPosHookHandle,
    //       pl::hook::PriorityNormal);
    // ---------------------------------------------------------------
    self.getLogger().debug("[MiniMap::enable] Player-position hook is a placeholder.");

    // ---------------------------------------------------------------
    // 占位 Hook 2：渲染（eglSwapBuffers 或 Minecraft 的 swap 函数）
    // 目标：eglSwapBuffers 或 游戏内 render 函数
    //
    // 安装示例：
    //   void *eglSwapAddr = dlsym(RTLD_DEFAULT, "eglSwapBuffers");
    //   pl::hook::hook(
    //       reinterpret_cast<pl::hook::FuncPtr>(eglSwapAddr),
    //       reinterpret_cast<pl::hook::FuncPtr>(+[](EGLDisplay dpy, EGLSurface surf) -> EGLBoolean {
    //           auto &mod = MyMod::getInstance();
    //           // 先调用原始 swap
    //           EGLBoolean ret = original_eglSwapBuffers(dpy, surf);
    //           // 再绘制小地图覆盖层
    //           mod.drawOverlay();
    //           return ret;
    //       }),
    //       &mRenderHookHandle,
    //       pl::hook::PriorityNormal);
    // ---------------------------------------------------------------
    self.getLogger().debug("[MiniMap::enable] Render hook is a placeholder.");

    // ---------------------------------------------------------------
    // 占位 Hook 3：触摸输入
    // 目标：游戏的触摸处理函数（如 Minecraft::onTouchEvent 或 AInputQueue）
    //
    // 安装示例：
    //   void *touchAddr = /* 游戏内触摸分发函数 */;
    //   pl::hook::hook(..., +[](float x, float y, bool down) {
    //       MyMod::getInstance().onTouchEvent(x, y, down);
    //       // 继续传递给游戏
    //       original_touch(x, y, down);
    //   }, ...);
    // ---------------------------------------------------------------
    self.getLogger().debug("[MiniMap::enable] Touch hook is a placeholder.");

    if (!installHooks()) {
        self.getLogger().error("[MiniMap::enable] Hook installation failed.");
        // 不阻断 enable，允许降级运行
    }

    self.getLogger().info(
        "[MiniMap::enable] Enabled. Tap the map button in the "
        "top‑right corner to open the minimap.");
    return true;
}

// ============================================================================
// disable() — 卸载 Hook
// ============================================================================
bool MyMod::disable() {
    auto &self = getSelf();
    self.getLogger().debug("[MiniMap::disable] Removing hooks...");

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        config.$minimapOpen = false;
    }

    if (!uninstallHooks()) {
        self.getLogger().warn("[MiniMap::disable] Some hooks not cleanly removed.");
    }

    self.getLogger().info("[MiniMap::disable] Disabled.");
    return true;
}

// ============================================================================
// unload() — 最终清理
// ============================================================================
bool MyMod::unload() {
    auto &self = getSelf();
    self.getLogger().debug("[MiniMap::unload] Cleaning up...");
    uninstallHooks();
    self.getLogger().info("[MiniMap::unload] Unloaded.");
    return true;
}

// ============================================================================
// Hook 安装 / 卸载（占位）
// ============================================================================
bool MyMod::installHooks() {
    // 真实实现：在此调用 pl::hook::hook(...)
    return true;
}

bool MyMod::uninstallHooks() {
    // 真实实现：在此调用 pl::hook::unhook(...)
    mPosHookHandle = nullptr;
    mRenderHookHandle = nullptr;
    mTouchHookHandle = nullptr;
    return true;
}

// ============================================================================
// 玩家位置更新（由位置 Hook 回调）
// ============================================================================
void MyMod::onPlayerPositionUpdated(float x, float y, float z, float yaw) {
    std::lock_guard<std::mutex> lock(stateMutex);
    playerPos = {x, y, z};
    playerYaw = yaw;
}

// ============================================================================
// 屏幕尺寸变化（由渲染 Hook 或系统回调触发）
// ============================================================================
void MyMod::onScreenSizeChanged(int w, int h) {
    std::lock_guard<std::mutex> lock(stateMutex);
    config.$screenW = w;
    config.$screenH = h;
}

// ============================================================================
// 触摸事件处理（由触摸 Hook 回调）
// ============================================================================
void MyMod::onTouchEvent(float screenX, float screenY, bool down) {
    if (!down) return; // 仅处理按下事件

    std::lock_guard<std::mutex> lock(stateMutex);
    auto &self = getSelf();

    const int scrW = config.$screenW;
    const int scrH = config.$screenH;
    if (scrW == 0 || scrH == 0) return;

    // ---- 1. 检测开关按钮 ----
    float btnCx = config.btnX * scrW;
    float btnCy = config.btnY * scrH;
    float btnR  = config.btnSize * 0.5f;

    if (isInsideCircle(screenX, screenY, btnCx, btnCy, btnR)) {
        config.$minimapOpen = !config.$minimapOpen;
        self.getLogger().info(
            "[MiniMap] Minimap {}.", config.$minimapOpen ? "opened" : "closed");
        return; // 按钮事件已消费
    }

    // ---- 2. 如果小地图未打开，忽略后续交互 ----
    if (!config.$minimapOpen) return;

    // ---- 3. 计算小地图中心（屏幕右上区域） ----
    float mapCenterX = scrW - config.mapSize * 0.5f - 20.0f;
    float mapCenterY = config.mapSize * 0.5f + 60.0f;
    float mapR = config.mapSize * 0.5f;

    // 点击是否在小地图圆形区域内？
    if (!isInsideCircle(screenX, screenY, mapCenterX, mapCenterY, mapR)) {
        return; // 点击在小地图外部，忽略
    }

    // ---- 4. 将屏幕坐标映射到世界坐标 ----
    float mx = 0.0f, my = 0.0f;
    screenToMinimap(screenX, screenY, mx, my);

    // 计算世界坐标
    float worldX, worldZ;
    if (config.rotateWithPlayer) {
        float rad = -playerYaw * (M_PI / 180.0f);
        float rx = mx * std::cos(rad) - my * std::sin(rad);
        float ry = mx * std::sin(rad) + my * std::cos(rad);
        worldX = playerPos.x + rx;
        worldZ = playerPos.z + ry;
    } else {
        worldX = playerPos.x + mx;
        worldZ = playerPos.z + my;
    }
    float worldY = playerPos.y;

    // ---- 5. 检测是否点击到已有传送点（5 像素容差） ----
    const float tapToleranceBlocks = 5.0f * config.mapScale; // 5px → 世界距离
    for (size_t i = 0; i < config.waypoints.size(); ++i) {
        const auto &wp = config.waypoints[i];
        // 计算传送点在小地图上的屏幕坐标
        float wpDx = wp.x - playerPos.x;
        float wpDz = wp.z - playerPos.z;
        if (config.rotateWithPlayer) {
            float rad = playerYaw * (M_PI / 180.0f);
            float rx = wpDx * std::cos(rad) - wpDz * std::sin(rad);
            float ry = wpDx * std::sin(rad) + wpDz * std::cos(rad);
            wpDx = rx; wpDz = ry;
        }
        float wpSx = mapCenterX + wpDx / config.mapScale;
        float wpSy = mapCenterY - wpDz / config.mapScale;

        float distPx = std::hypot(screenX - wpSx, screenY - wpSy);
        if (distPx < 18.0f) { // 18px 命中半径
            self.getLogger().info(
                "[MiniMap] Tapped waypoint '{}'. Teleporting...", wp.name);
            teleportToWaypoint(wp);
            return;
        }
    }

    // ---- 6. 否则：在空白处创建新传送点 ----
    char nameBuf[64];
    std::snprintf(nameBuf, sizeof(nameBuf), "WP_%.0f_%.0f_%.0f",
                  worldX, worldY, worldZ);

    addWaypoint(std::string(nameBuf), worldX, worldY, worldZ);
    self.getLogger().info(
        "[MiniMap] Created waypoint '{}' at ({:.0f}, {:.0f}, {:.0f})",
        nameBuf, worldX, worldY, worldZ);
}

// ============================================================================
// 屏幕坐标 → 小地图相对坐标（以玩家为中心，方块单位）
// ============================================================================
void MyMod::screenToMinimap(float sx, float sy, float &mx, float &my) {
    const int scrW = config.$screenW;
    const int scrH = config.$screenH;
    float mapCenterX = scrW - config.mapSize * 0.5f - 20.0f;
    float mapCenterY = config.mapSize * 0.5f + 60.0f;

    float dx = (sx - mapCenterX) * config.mapScale; // 世界 X 偏移
    float dy = (mapCenterY - sy) * config.mapScale; // 世界 Z 偏移（屏幕Y翻转）

    mx = dx;
    my = dy;
}

// ============================================================================
// 判断点是否在圆内
// ============================================================================
bool MyMod::isInsideCircle(float px, float py, float cx, float cy, float r) {
    return (px - cx) * (px - cx) + (py - cy) * (py - cy) <= r * r;
}

// ============================================================================
// 解析 Hex 颜色 "#RRGGBB"
// ============================================================================
bool MyMod::parseHexColor(const std::string &hex, uint8_t &r, uint8_t &g, uint8_t &b) {
    if (hex.size() != 7 || hex[0] != '#') return false;
    unsigned int val;
    std::stringstream ss;
    ss << std::hex << hex.substr(1);
    ss >> val;
    if (ss.fail()) return false;
    r = (val >> 16) & 0xFF;
    g = (val >> 8)  & 0xFF;
    b = val & 0xFF;
    return true;
}

// ============================================================================
// 绘制覆盖层（在 eglSwapBuffers 之后调用）
// ============================================================================
void MyMod::drawOverlay() {
    std::lock_guard<std::mutex> lock(stateMutex);

    const int scrW = config.$screenW;
    const int scrH = config.$screenH;
    if (scrW == 0 || scrH == 0) return;

    // ---- 保存 GL 状态（占位） ----
    // GLboolean blendWasOn = glIsEnabled(GL_BLEND);
    // glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ================================================================
    // 1. 始终绘制开关按钮（屏幕右上角）
    // ================================================================
    float btnCx = config.btnX * scrW;
    float btnCy = config.btnY * scrH;
    float btnR  = config.btnSize * 0.5f;
    uint8_t btnA = static_cast<uint8_t>(config.btnAlpha * 255);

    // 按钮底色
    drawFilledCircle(btnCx, btnCy, btnR, 40, 40, 40, btnA, 24);
    // 按钮边框
    drawCircle(btnCx, btnCy, btnR, 180, 180, 180, btnA, 24);

    // 按钮图标："M" 字母简化为中心圆点 + 小三角
    if (config.$minimapOpen) {
        // 小地图已打开 → 显示 "X" 形状（两条交叉线）
        float cross = btnR * 0.35f;
        // 这里用简化方式：四个小点构成 X
        drawFilledCircle(btnCx - cross, btnCy - cross, btnR * 0.15f, 255, 80, 80, 220, 8);
        drawFilledCircle(btnCx + cross, btnCy - cross, btnR * 0.15f, 255, 80, 80, 220, 8);
        drawFilledCircle(btnCx - cross, btnCy + cross, btnR * 0.15f, 255, 80, 80, 220, 8);
        drawFilledCircle(btnCx + cross, btnCy + cross, btnR * 0.15f, 255, 80, 80, 220, 8);
    } else {
        // 小地图关闭 → 显示 "□" 形状（空心方块）
        float sq = btnR * 0.4f;
        // 简化：四个角 + 中心点
        drawFilledCircle(btnCx, btnCy, btnR * 0.22f, 100, 200, 255, 210, 12);
    }

    // ================================================================
    // 2. 如果小地图打开，绘制完整小地图
    // ================================================================
    if (!config.$minimapOpen) {
        // 恢复 GL 状态 & 返回
        // if (!blendWasOn) glDisable(GL_BLEND);
        return;
    }

    // --- 小地图中心 ---
    float mapCenterX = scrW - config.mapSize * 0.5f - 20.0f;
    float mapCenterY = config.mapSize * 0.5f + 60.0f;
    float mapR = config.mapSize * 0.5f;
    uint8_t mapBgA = static_cast<uint8_t>(config.mapAlpha * 255);

    // --- 2a. 背景圆 ---
    drawFilledCircle(mapCenterX, mapCenterY, mapR, 10, 10, 15, mapBgA, 48);

    // --- 2b. 边框 ---
    drawCircle(mapCenterX, mapCenterY, mapR, 200, 200, 200, mapBgA, 48);

    // --- 2c. 方向指示（北 = 上） ---
    float dirLen = mapR * 0.85f;
    float northAngle = config.rotateWithPlayer ? (playerYaw * M_PI / 180.0f) : 0.0f;
    for (int i = 0; i < 4; ++i) {
        float a = northAngle + i * M_PI / 2.0f;
        float tx = mapCenterX + std::sin(a) * dirLen;
        float ty = mapCenterY - std::cos(a) * dirLen;
        drawFilledCircle(tx, ty, 3.0f, 200, 200, 200, 200, 8);
    }
    // 北方向特殊标记（红色）
    {
        float nx = mapCenterX + std::sin(northAngle) * dirLen;
        float ny = mapCenterY - std::cos(northAngle) * dirLen;
        drawFilledCircle(nx, ny, 5.0f, 255, 50, 50, 230, 10);
    }

    // --- 2d. 玩家位置（中心三角箭头） ---
    float arrowAngle = config.rotateWithPlayer ? 0.0f : (playerYaw * M_PI / 180.0f);
    drawTriangle(mapCenterX, mapCenterY, 10.0f, arrowAngle, 255, 255, 255, 240);

    // --- 2e. 传送点 ---
    for (const auto &wp : config.waypoints) {
        // 世界偏移 → 屏幕偏移
        float dx = wp.x - playerPos.x;
        float dz = wp.z - playerPos.z;

        if (config.rotateWithPlayer) {
            float rad = playerYaw * (M_PI / 180.0f);
            float rx = dx * std::cos(rad) - dz * std::sin(rad);
            float ry = dx * std::sin(rad) + dz * std::cos(rad);
            dx = rx; dz = ry;
        }

        float sx = mapCenterX + dx / config.mapScale;
        float sy = mapCenterY - dz / config.mapScale;

        // 裁剪到小地图范围内
        float distFromCenter = std::hypot(sx - mapCenterX, sy - mapCenterY);
        if (distFromCenter > mapR - 6.0f) {
            // 方向指示（在小地图边缘显示小色点）
            float edgeAngle = std::atan2(sy - mapCenterY, sx - mapCenterX);
            sx = mapCenterX + std::cos(edgeAngle) * (mapR - 6.0f);
            sy = mapCenterY + std::sin(edgeAngle) * (mapR - 6.0f);
        }

        // 解析颜色
        uint8_t wr = 255, wg = 80, wb = 80;
        parseHexColor(wp.color, wr, wg, wb);

        // 外圈（半透明）
        drawFilledCircle(sx, sy, 7.0f, wr, wg, wb, 140, 12);
        // 内核
        drawFilledCircle(sx, sy, 3.5f, wr, wg, wb, 240, 8);
    }

    // --- 2f. 坐标文字（简化：左下角显示数字） ---
    // 由于原生环境下没有字体渲染，用坐标轴标记替代：
    // X+ → 右侧红点  Z+ → 上方绿点
    {
        float labelR = mapR * 0.8f;
        drawFilledCircle(mapCenterX + labelR, mapCenterY, 4.0f, 255, 60, 60, 200, 8); // E
        drawFilledCircle(mapCenterX - labelR, mapCenterY, 4.0f, 60, 60, 255, 200, 8); // W
        drawFilledCircle(mapCenterX, mapCenterY - labelR, 4.0f, 60, 255, 60, 200, 8); // S (if rotate)
    }

    // ---- 恢复 GL 状态（占位） ----
    // if (!blendWasOn) glDisable(GL_BLEND);
}

// ============================================================================
// OpenGL ES 绘制原语（占位实现 — 需要实际的 GLES 调用）
// ============================================================================
void MyMod::drawCircle(float cx, float cy, float r,
                       uint8_t r, uint8_t g, uint8_t b, uint8_t a, int segments) {
    // 占位：实际应使用 GL_LINE_LOOP 绘制
    // 伪代码：
    //   std::vector<float> verts;
    //   for (int i = 0; i <= segments; ++i) {
    //       float angle = 2.0f * M_PI * i / segments;
    //       verts.push_back(cx + r * cos(angle));
    //       verts.push_back(cy + r * sin(angle));
    //   }
    //   glEnableVertexAttribArray(0);
    //   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts.data());
    //   glUniform4f(colorLoc, r/255.f, g/255.f, b/255.f, a/255.f);
    //   glDrawArrays(GL_LINE_LOOP, 0, segments + 1);
    (void)cx; (void)cy; (void)r; (void)segments;
}

void MyMod::drawFilledCircle(float cx, float cy, float r,
                             uint8_t r, uint8_t g, uint8_t b, uint8_t a, int segments) {
    // 占位：实际应使用 GL_TRIANGLE_FAN 绘制
    (void)cx; (void)cy; (void)r; (void)segments;
}

void MyMod::drawTriangle(float cx, float cy, float size, float angle,
                         uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // 占位：实际应使用 GL_TRIANGLES 绘制
    (void)cx; (void)cy; (void)size; (void)angle;
}

// ============================================================================
// 传送点管理
// ============================================================================
void MyMod::addWaypoint(const std::string &name, float x, float y, float z) {
    std::lock_guard<std::mutex> lock(stateMutex);
    Waypoint wp;
    wp.name = name;
    wp.x = x;
    wp.y = y;
    wp.z = z;
    wp.dimension = 0;
    wp.color = "#FF4444";
    config.waypoints.push_back(wp);
    saveConfig();
}

void MyMod::removeWaypoint(size_t index) {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (index < config.waypoints.size()) {
        config.waypoints.erase(config.waypoints.begin() + index);
        saveConfig();
    }
}

// ============================================================================
// 传送到传送点
// ============================================================================
bool MyMod::teleportToWaypoint(const Waypoint &wp) {
    auto &self = getSelf();

    // ---------------------------------------------------------------
    // 占位：实际实现需要 Hook 游戏内部传送函数或发送命令
    //
    // 方案 A（推荐）：Hook ServerCommands::executeCommand
    //   构造命令字符串："/tp @p {:.1f} {:.1f} {:.1f}"
    //   通过游戏命令系统执行
    //
    // 方案 B：直接修改 Actor 坐标
    //   void *player = /* 获取 LocalPlayer 指针 */;
    //   // 调用 Actor::setPos(Vec3{wp.x, wp.y, wp.z})
    //
    // 方案 C：通过 JNI 调用游戏方法
    //   JNIEnv *env;
    //   getSelf().getJavaVM()->AttachCurrentThread(&env, nullptr);
    //   // 调用 Minecraft 的 Java 层传送方法
    // ---------------------------------------------------------------

    self.getLogger().info(
        "[MiniMap::Teleport] Teleporting to '{}' at ({:.1f}, {:.1f}, {:.1f}) "
        "[placeholder — implement via command hook or direct Actor::setPos]",
        wp.name, wp.x, wp.y, wp.z);

    // 实际传送代码（需要 Hook 后启用）：
    // char cmd[128];
    // std::snprintf(cmd, sizeof(cmd), "/tp @p %.1f %.1f %.1f", wp.x, wp.y, wp.z);
    // executeGameCommand(cmd);

    return true;
}

// ============================================================================
// 保存配置
// ============================================================================
bool MyMod::saveConfig() {
    // 克隆一份去掉 $ 运行时字段后保存
    MinimapConfig toSave = config;
    toSave.$minimapOpen = false; // 不持久化运行时状态
    toSave.$screenW = 1080;
    toSave.$screenH = 1920;

    pl::config::ConfigFile<MinimapConfig> configFile(toSave);
    return configFile.save();
}

} // namespace my_mod
