cmake_minimum_required(VERSION 3.31)
project(VistualMouse LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(gamepwnage.cmake)
include_directories(
        ${CMAKE_SOURCE_DIR}/ImGui
        ${CMAKE_SOURCE_DIR}/ImGui/backends
)

include_directories(gamepwnage/includes)

set(IMGUI
        ImGui/imgui.cpp
        ImGui/imgui_draw.cpp
        ImGui/imgui_tables.cpp
        ImGui/imgui_widgets.cpp
        ImGui/backends/imgui_impl_opengl3.cpp
        ImGui/backends/imgui_impl_android.cpp
)
add_library(${CMAKE_PROJECT_NAME} SHARED
        main.cpp
        cursor.h
        cursor_render.cpp
        stub.h
        AmbientTools.h
        ${IMGUI}
)


target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
        -g0
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)

add_library(nise INTERFACE IMPORTED)


set_property(TARGET nise PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}
)

include(FetchContent)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

# stb_image is header-only; create a small impl target
add_library(stb_image STATIC stb_image_impl.cpp)
target_include_directories(stb_image PUBLIC ${stb_SOURCE_DIR})

set_property(TARGET nise PROPERTY
        INTERFACE_LINK_LIBRARIES
        gamepwnage
        "-L${CMAKE_CURRENT_LIST_DIR}/libs/${ANDROID_ABI} -lnise"
)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
        "-Wl,-z,max-page-size=16384"
        "-Wl,-z,common-page-size=16384"
)


target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${stb_SOURCE_DIR}
        ${GAMEPWNAGE_DIR}/src
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        gamepwnage
        stb_image
        android
        log
        nise
        GLESv3 EGL)