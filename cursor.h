#pragma once
#include <atomic>
#include <string>

namespace VirtualMouse {

extern std::atomic<bool> visible;
extern float cursorX;
extern float cursorY;
extern float screenW;
extern float screenH;

// PNG texture handle (ImGui/OpenGL)
extern unsigned int textureId;
extern bool textureLoaded;

static constexpr const char* CURSOR_PNG_PATH =
    "/storage/emulated/0/Android/media/io.kitsuri.mayape/modules/virtualmouse/cursor.png";
static constexpr float CURSOR_SIZE = 32.0f;

void init();
void update(float w, float h);

} // namespace VirtualMouse
