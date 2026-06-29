#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"
#include <EGL/egl.h>
#include "stub.h"

#include "cursor.h"
#include "imgui.h"
#include "stb_image.h"
#include <GLES3/gl3.h>
#include <android/log.h>

#define TAG "VirtualMouse"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

namespace VirtualMouse {

unsigned int textureId   = 0;
bool         textureLoaded = false;

static bool loadTexture() {
    int w, h, channels;
    unsigned char* data = stbi_load(CURSOR_PNG_PATH, &w, &h, &channels, 4);
    if (!data) {
        LOGD("Failed to load cursor PNG: %s", CURSOR_PNG_PATH);
        return false;
    }

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);

    LOGD("Cursor texture loaded (%dx%d)", w, h);
    return true;
}

void render() {
    if (!visible.load()) return;

    if (!textureLoaded) {
        textureLoaded = loadTexture();
        if (!textureLoaded) return;
    }

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddImage(
        (ImTextureID)(uintptr_t)textureId,
        ImVec2(cursorX, cursorY),
        ImVec2(cursorX + CURSOR_SIZE, cursorY + CURSOR_SIZE)
    );
}

} // namespace VirtualMouse
