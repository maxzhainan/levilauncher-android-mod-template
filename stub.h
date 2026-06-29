#pragma once


#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <functional>



namespace FMOD {
    class System;
    class Sound;
    class Channel;
    class ChannelControl;
    class ChannelGroup;
}
struct FMOD_CREATESOUNDEXINFO;



/**
 * @brief Hooks into the FMOD Core library loaded by Minecraft to intercept
 *        audio stream creation, playback, and channel management.
 *
 * Hooks installed:
 *  - FMOD::System::createStream   – intercepts file-open calls, enabling path overrides
 *  - FMOD::System::playSound      – tracks which channel a sound is playing on
 *  - FMOD::Sound::release         – cleans up tracking state when a sound is freed
 *  - FMOD::ChannelControl::stop   – detects when playback stops
 *  - FMOD::ChannelControl::setPaused – detects pause/resume state changes
 */
class FMODHook {
public:

    /**
     * @brief Runtime state of a tracked FMOD sound stream.
     */
    struct TrackInfo {
        std::string    original_path;  ///< Path as received by createStream (before override).
        std::string    override_path;  ///< Path actually opened after applying any override; empty if no override.
        FMOD::Sound*   sound;          ///< FMOD sound object for this stream.
        FMOD::Channel* channel;        ///< Channel the sound is (or was last) playing on; nullptr if not yet played.
        bool           is_playing;     ///< true if the channel is currently playing (not paused, not stopped).
        bool           is_paused;      ///< true if the channel is currently paused.
        float          volume;         ///< Last-known channel volume in [0.0, 1.0].

        TrackInfo();
    };

    /**
     * @brief Callback fired on the FMOD audio thread whenever the active track
     *        changes (new track starts, or playback stops entirely).
     *
     * Keep the handler short and thread-safe.
     *
     * @param path     Path of the new track (before any override), or empty if
     *                 playback stopped.
     * @param sound    FMOD::Sound* for the new track; nullptr if playback stopped.
     * @param channel  FMOD::Channel* the new track is playing on; nullptr if stopped.
     */
    using TrackChangeCallback = std::function<void(const std::string& path,
                                                   FMOD::Sound*       sound,
                                                   FMOD::Channel*     channel)>;

    // ----------------------------------------------------------
    //  Singleton lifecycle
    // ----------------------------------------------------------

    /**
     * @brief Returns the process-wide FMODHook instance.
     * @return Reference to the singleton.
     */
    static FMODHook& getInstance();


    // ----------------------------------------------------------
    //  Path overrides
    // ----------------------------------------------------------

    /**
     * @brief Redirects an FMOD audio file open from @p original_path to
     *        @p custom_path on disk.
     *
     * Whenever FMOD's createStream hook intercepts a call where the path
     * matches @p original_path, it transparently substitutes @p custom_path
     * so FMOD opens the replacement file instead.
     *
     * @param original_path  Path string as Minecraft passes it to createStream
     *                       (e.g. "assets/music/game/creative.ogg").
     * @param custom_path    Absolute path on the device to serve instead
     *                       (e.g. "/sdcard/MyMod/music/creative.ogg").
     */
    void addPathOverride(const std::string& original_path, const std::string& custom_path);

    /**
     * @brief Removes a single path override registered by addPathOverride().
     *
     * After this call, @p original_path is forwarded to FMOD unmodified.
     * Has no effect if the path was never registered.
     *
     * @param original_path  The original path key to remove.
     */
    void removePathOverride(const std::string& original_path);

    /**
     * @brief Removes all registered path overrides.
     *
     * Equivalent to calling removePathOverride() for every registered entry.
     */
    void clearPathOverrides();

    // ----------------------------------------------------------
    //  Playback control
    // ----------------------------------------------------------

    /**
     * @brief Pauses the channel of the currently tracked active track.
     *
     * Internally calls FMOD::ChannelControl::setPaused(true) via the original
     * (non-hooked) function pointer. Has no effect if no track is playing.
     *
     * @return true if a playing track was found and paused successfully.
     */
    bool pauseCurrentTrack();

    /**
     * @brief Resumes the channel of the currently tracked active track.
     *
     * Calls FMOD::ChannelControl::setPaused(false). Has no effect if no
     * track is paused.
     *
     * @return true if a paused track was found and resumed successfully.
     */
    bool resumeCurrentTrack();

    /**
     * @brief Stops only the currently tracked active track.
     *
     * Calls FMOD::ChannelControl::stop() on the current channel. The track
     * cannot be resumed after this.
     *
     * @return true if an active track was found and stopped successfully.
     */
    bool stopCurrentTrack();

    /**
     * @brief Stops every FMOD channel that is currently playing.
     *
     * More aggressive than stopCurrentTrack(); affects all channels managed by
     * Minecraft's FMOD instance, not just the one tracked internally.
     *
     * @return true if the stop command was dispatched successfully.
     */
    bool stopAll();

    // ----------------------------------------------------------
    //  Track information
    // ----------------------------------------------------------

    /**
     * @brief Returns a read-only pointer to the TrackInfo for the currently
     *        active sound stream.
     *
     * The pointer is valid until the next track change event or until cleanup()
     * is called. Do not store it beyond the scope of the current call.
     *
     * @return Pointer to the current TrackInfo; nullptr if no track is active.
     */
    const TrackInfo* getCurrentTrack() const;

    /**
     * @brief Returns the original (pre-override) path of the current track.
     *
     * @return Original path string; empty string if no track is active.
     */
    std::string getCurrentTrackPath() const;

    /**
     * @brief Returns whether a track is currently in the playing (non-paused,
     *        non-stopped) state.
     *
     * @return true if is_playing is true on the current TrackInfo.
     */
    bool isTrackPlaying() const;

    // ----------------------------------------------------------
    //  Volume
    // ----------------------------------------------------------

    /**
     * @brief Sets the volume on the currently active channel.
     *
     * Calls FMOD::ChannelControl::setVolume() via the original function pointer.
     * Values outside [0.0, 1.0] are clamped by FMOD.
     *
     * @param volume  Desired volume level in [0.0, 1.0].
     * @return true if a current channel was found and the volume was set.
     */
    bool setVolume(float volume);

    /**
     * @brief Returns the last-known volume of the current track.
     *
     * @return Volume in [0.0, 1.0], or -1.0f if no track is active or the
     *         hook is not initialised.
     */
    float getVolume() const;

    // ----------------------------------------------------------
    //  Callbacks
    // ----------------------------------------------------------

    /**
     * @brief Registers a callback invoked whenever the active track changes.
     *
     * The callback fires on the FMOD audio thread when:
     *  - A new stream is opened and begins playing.
     *  - The current channel is stopped or released.
     *
     * Replaces any previously registered callback. The handler must be
     * short and thread-safe.
     *
     * @param callback  Function matching the TrackChangeCallback signature.
     *                  Receives the new track's original path, Sound*, and Channel*,
     *                  or empty/nullptr values when playback stops.
     */
    void setTrackChangeCallback(TrackChangeCallback callback);

    /**
     * @brief Removes the callback registered by setTrackChangeCallback().
     *
     * Safe to call even if no callback was registered.
     */
    void clearTrackChangeCallback();

private:
    FMODHook()  = default;
    ~FMODHook() = default;
    FMODHook(const FMODHook&)            = delete;
    FMODHook& operator=(const FMODHook&) = delete;
};


// ============================================================
//  VirtualAssets
// ============================================================

/**
 * @defgroup VirtualAssets VirtualAssets API
 *
 * @brief Hooks the Android AAssetManager to layer a virtual filesystem
 *        transparently on top of the real APK asset tree.
 *
 * ### How the hook works
 * The NDK's AAssetManager_open (and its internal siblings) is intercepted via
 * HookManager. Every asset path opened by the game passes through the
 * interceptor, which checks the virtual registry before forwarding to the
 * real AAssetManager zip reader.
 *
 * ### Interception order (highest to lowest priority)
 *  1. **Blocked** – path returns failure immediately; the game sees the file
 *     as non-existent even though it is in the APK.
 *  2. **Virtual entry** – data registered by AddFile / AddTextFile / LoadDir /
 *     ReplaceFile is served to the caller instead of the APK data.
 *  3. **Fall-through** – the call is forwarded to the real AAssetManager and
 *     the APK data is returned unmodified.
 *
 * ### Thread safety
 * All functions are thread-safe after init_virtual_assets() returns. Internal
 * state is protected by a mutex; concurrent calls from the render thread,
 * audio thread, or mod worker threads are safe.
 *
 * @{
 */


/**
 * @brief Blocks an asset path so that the game cannot open it.
 *
 * Any AAssetManager_open call for @p path will return failure (null asset),
 * as if the file does not exist in the APK.
 *
 * Common uses:
 *  - Suppressing vanilla assets you are completely replacing.
 *  - Preventing a file from loading that crashes with your mod installed.
 *
 * Blocking has higher priority than virtual entries: even if a virtual entry
 * is registered for the same path, the file appears missing while blocked.
 * Lift the block with VirtualAssets_UnblockFile().
 *
 * @param path  Asset path relative to the APK assets root
 *              (e.g. "textures/terrain/grass.png"). Must not be nullptr.
 */
void VirtualAssets_BlockFile(const char* path);

/**
 * @brief Lifts a block previously applied by VirtualAssets_BlockFile().
 *
 * After this call the path resumes normal interception priority:
 * a virtual entry is served if one exists, otherwise the real APK data is returned.
 *
 * Has no effect if @p path is not currently blocked.
 *
 * @param path  Asset path to unblock; must match the string passed to BlockFile.
 */
void VirtualAssets_UnblockFile(const char* path);

/**
 * @brief Injects a binary blob as a virtual asset at @p path.
 *
 * The @p data buffer is copied into an internal store immediately; the caller
 * may free or reuse the buffer right after this call returns.
 *
 * If a virtual entry already exists at @p path it is replaced atomically.
 * If the path is currently blocked the entry is stored but invisible until
 * VirtualAssets_UnblockFile() is called.
 *
 * @param path  Virtual asset path to register (e.g. "shaders/glsl/terrain.vertex").
 * @param data  Raw bytes to inject. Must not be nullptr.
 * @param size  Number of bytes in @p data.
 */
void VirtualAssets_AddFile(const char* path, const void* data, size_t size);

/**
 * @brief Injects a UTF-8 text string as a virtual asset at @p path.
 *
 * Convenience wrapper around VirtualAssets_AddFile() for plain-text content
 * (JSON, XML, shader source, CSV, etc.). The content is copied internally; the
 * caller retains ownership of the @p content pointer. The null terminator is
 * NOT included in the stored byte count.
 *
 * @param path     Virtual asset path (e.g. "config/mod_settings.json").
 * @param content  Null-terminated UTF-8 string to store. Must not be nullptr.
 */
void VirtualAssets_AddTextFile(const char* path, const char* content);

/**
 * @brief Removes a virtual entry from the registry.
 *
 * After this call, opens for @p path fall through to the real AAssetManager
 * (or fail if the path is still blocked).
 *
 * Has no effect if @p path has no registered virtual entry.
 *
 * @param path  Virtual asset path to remove.
 */
void VirtualAssets_RemoveFile(const char* path);

/**
 * @brief Returns whether a virtual entry is registered for @p path.
 *
 * Returns true even if the path is currently blocked. Does NOT query whether
 * the file exists in the real APK.
 *
 * @param path  Virtual asset path to query.
 * @return true if a virtual entry exists for @p path.
 */
bool VirtualAssets_HasFile(const char* path);

/**
 * @brief Bulk-registers all files in an on-device directory as virtual assets.
 *
 * Walks @p storageDir on the real device filesystem and registers each file
 * as a virtual asset under @p virtualBaseDir, preserving relative subdirectory
 * structure.  Files are loaded **lazily** — the virtual entry stores the real
 * path and bytes are read only when the game actually opens the asset.  This
 * means you can edit files on storage and the changes are visible immediately
 * without calling LoadDir again.
 *
 * Example:
 * @code
 *   // Device:    /sdcard/MyMod/textures/grass.png
 *   // Served as: assets/textures/grass.png  in the virtual asset tree
 *   VirtualAssets_LoadDir("/sdcard/MyMod/textures", "textures", true);
 * @endcode
 *
 * @param storageDir      Absolute path to the source directory on the device
 *                        (e.g. "/sdcard/MyMod/assets"). Must exist and be readable.
 * @param virtualBaseDir  Prefix in the virtual asset tree where files are placed
 *                        (e.g. "textures/custom"). Pass "" to map directly under
 *                        the asset root.
 * @param recursive       true = traverse subdirectories and register their contents
 *                        recursively, preserving folder structure.
 *                        false = register only the immediate files in @p storageDir.
 * @return Number of files successfully registered; -1 if @p storageDir does not
 *         exist, is not a directory, or cannot be read.
 */
int VirtualAssets_LoadDir(const char* storageDir, const char* virtualBaseDir, bool recursive);

/**
 * @brief Points a virtual asset entry at an on-device file, replacing its contents.
 *
 * Creates or updates the virtual entry at @p virtualPath so that it is backed
 * by @p storagePath on the real filesystem. The file at @p storagePath is
 * read each time the asset is opened, so disk edits are visible immediately
 * without calling ReplaceFile again.
 *
 * Prefer this over VirtualAssets_AddFile() when the source data is a file on
 * device storage that may be updated while the game is running.
 *
 * @param virtualPath  Destination in the virtual asset tree
 *                     (e.g. "assets/textures/terrain/grass.png").
 * @param storagePath  Absolute path to the replacement file on the device
 *                     (e.g. "/sdcard/MyMod/grass.png"). Must be readable.
 * @return true if the virtual entry was created or updated successfully.
 *         false if @p storagePath cannot be accessed.
 */
bool VirtualAssets_ReplaceFile(const char* virtualPath, const char* storagePath);

/**
 * @brief Reads a virtual asset into a newly heap-allocated buffer.
 *
 * Only reads from the virtual registry — does NOT fall through to the real
 * AAssetManager. The caller is responsible for freeing the returned buffer
 * with free().
 *
 * @param path     Virtual asset path to read.
 * @param outSize  Out-param set to the size of the returned buffer in bytes.
 *                 Set to 0 on failure. Must not be nullptr.
 * @return Pointer to a malloc'd buffer containing the raw file bytes on success;
 *         nullptr if @p path is not in the virtual registry, is blocked, or
 *         memory allocation fails. Caller must free() the pointer.
 */
void* VirtualAssets_ReadFile(const char* path, size_t* outSize);

/**
 * @brief Overwrites the stored bytes of an existing virtual asset entry.
 *
 * Replaces the internally held data for @p path with @p data. The new bytes
 * are copied internally; the caller retains ownership of @p data.
 *
 * The entry at @p path must already exist. If it does not, call
 * VirtualAssets_AddFile() first to create it.
 *
 * @param path  Virtual asset path of the entry to update.
 * @param data  Pointer to the new raw bytes. Must not be nullptr.
 * @param size  Number of bytes in @p data.
 */
void VirtualAssets_EditFile(const char* path, const void* data, size_t size);

/** @} */ // end of VirtualAssets group


// ============================================================
//  Miscellaneous utilities
// ============================================================

/**
 * @brief Returns a pointer to the active EGL rendering context.
 *
 * Tracked by the egl_hook layer. Useful when a mod needs to issue raw
 * OpenGL/ES calls that must execute on the render context (creating textures,
 * sampling the framebuffer, etc.).
 *
 * @return Opaque pointer to the current EGLContext; nullptr if the render
 *         context has not been set up yet or the EGL hook is not installed.
 */
void* GetGLContextPointer();

/**
 * @brief Returns the renderer's current frames-per-second measurement.
 *
 * Read from the same frame-timing counter Minecraft uses internally, so the
 * value reflects actual rendered frames rather than a fixed game-tick rate.
 *
 * @return Instantaneous FPS as a float (e.g. 59.94f); 0.0f if no frame has
 *         been rendered yet.
 */
float GetCurrentFPS();

/**
 * @brief Function signature for render callbacks.
 *
 * Called once per frame from the engine's render thread, inside the active
 * OpenGL ES context (during eglSwapBuffers).
 *
 * This is the correct place to issue rendering commands such as ImGui draw calls
 * or custom OpenGL rendering. The GL context is guaranteed to be current.
 *
 * @note Do not perform heavy blocking operations inside this callback.
 */
typedef void (*RenderCallback)();


namespace RenderAPI {

    /**
     * @brief Registers a render callback.
     *
     * The callback will be invoked every frame during the rendering phase,
     * after the game has finished its draw calls but before buffers are swapped.
     *
     * Multiple callbacks may be registered and will be executed in registration order.
     *
     * @param cb Function pointer to a render callback.
     *
     * @note The callback must remain valid for the duration of its registration.
     * @note Duplicate registrations are ignored or may result in multiple calls
     *       depending on implementation.
     */
    void Register(RenderCallback cb);


    /**
     * @brief Unregisters a previously registered render callback.
     *
     * Removes the callback from the render pipeline so it will no longer be invoked.
     *
     * @param cb Function pointer previously passed to Register().
     *
     * @note Safe to call even if the callback is not currently registered.
     */
    void Unregister(RenderCallback cb);


    /**
     * @brief Registers a render callback beneath the game's UI layer.
     *
     * The callback will be invoked every frame during rendering,
     * before the game's UI and overlay elements are drawn.
     *
     * This is useful for rendering background visuals, world-space overlays,
     * or effects that should appear underneath menus and HUD components.
     *
     * Multiple callbacks may be registered and will execute in registration order.
     *
     * @param cb Function pointer to a render callback.
     *
     * @note The callback must remain valid for the duration of its registration.
     * @note Duplicate registrations are ignored or may result in multiple calls
     *       depending on implementation.
     */
    void RegisterUnderUI(RenderCallback cb);


    /**
     * @brief Unregisters a previously registered under-UI render callback.
     *
     * Removes the callback from the under-UI render pipeline
     * so it will no longer be invoked.
     *
     * @param cb Function pointer previously passed to RegisterUnderUI().
     *
     * @note Safe to call even if the callback is not currently registered.
     */
    void UnregisterUnderUI(RenderCallback cb);

}

/**
 * @brief Emits a log message through the Apps client logger.
 *
 *
 * @param threadName  Human-readable name of the calling thread, used as a log
 *                    prefix (e.g. "RenderThread", "ModInit"). Does not need to
 *                    match the OS thread name.
 * @param tag         Category tag for log filtering
 *                    (e.g. "VirtualAssets", "MyMod", "HookManager").
 * @param message     Null-terminated UTF-8 message string to emit.
 */
extern "C" void ClientLog(const char* threadName, const char* tag, const char* message);



#pragma once

/**
 * @brief Represents a single touch input event.
 *
 * Encapsulates data from the Android input system. Coordinates are provided
 * in screen space (pixels), matching the current surface resolution.
 */
struct TouchEvent {

    /**
     * @brief Action type of the touch event.
     *
     * Common values:
     * - 0 → ACTION_DOWN
     * - 1 → ACTION_UP
     * - 2 → ACTION_MOVE
     * - 5 → ACTION_POINTER_DOWN
     * - 6 → ACTION_POINTER_UP
     */
    int action;

    /**
     * @brief Pointer identifier for Multi touch input.
     *
     * Each active finger is assigned a unique pointerId. This allows tracking
     * multiple simultaneous touches.
     */
    int pointerId;

    /**
     * @brief X coordinate of the touch event (in pixels).
     */
    float x;

    /**
     * @brief Y coordinate of the touch event (in pixels).
     */
    float y;
};


/**
 * @brief Function signature for touch callbacks.
 *
 * Called whenever a touch event is received from the input system.
 *
 * @param ev Pointer to the current touch event.
 * @return true if the event is consumed and should NOT be passed to the game,
 *         false to allow the game to process it normally.
 *
 * @note Returning true will block the event from reaching the underlying game.
 */
typedef bool (*TouchCallback)(const TouchEvent* ev);


namespace TouchAPI {

    /**
     * @brief Registers a touch callback.
     *
     * The callback will be invoked for every incoming touch event.
     * Callbacks are executed in registration order.
     *
     * @param cb Function pointer to a touch callback.
     *
     * @note The callback must remain valid for the duration of its registration.
     */
    void RegisterCallback(TouchCallback cb);


    /**
     * @brief Unregisters a previously registered touch callback.
     *
     * Removes the callback from the input pipeline.
     *
     * @param cb Function pointer previously passed to RegisterCallback().
     *
     * @note Safe to call even if the callback is not currently registered.
     */
    void UnregisterCallback(TouchCallback cb);



}

#pragma once

// ============================================================
// Ambient SDK - Key Input API
// ============================================================
//
// Provides access to keyboard / button input from the engine.
// Supports interception and consumption of key events.
//
// ============================================================


/**
 * @brief Function signature for key input callbacks.
 *
 * Called whenever a key event is received from the Android input system.
 *
 * @param keyCode Android key code (e.g. KEYCODE_A, KEYCODE_BACK, etc.)
 * @param action  Key action:
 *                - 0 → ACTION_DOWN
 *                - 1 → ACTION_UP
 *                - 2 → ACTION_MULTIPLE
 * @param unicodeChar Unicode character produced by the key event (if applicable),
 *                    or 0 if the key does not produce a character.
 *
 * @return true if the event is consumed and should NOT be passed to the game,
 *         false to allow the game to process the event normally.
 *
 * @note Returning true blocks the event from reaching the underlying game.
 * @note This callback is executed on the input (JNI) thread.
 * @note Keep handlers lightweight to avoid input lag.
 */
typedef bool (*KeyHandler)(int keyCode, int action, int unicodeChar);




namespace KeyAPI {

    /**
     * @brief Registers a key input handler.
     *
     * The handler will be invoked for every key event dispatched
     * through the engine's input system.
     *
     * Handlers are executed in registration order.
     * Dispatch stops early if a handler returns true (event consumed).
     *
     * @param handler Function pointer to a key handler.
     *
     * @note The handler must remain valid for the duration of its registration.
     * @note Duplicate registrations are ignored.
     */
    void RegisterHandler(KeyHandler handler);


    /**
     * @brief Unregisters a previously registered key input handler.
     *
     * Removes the handler from the input pipeline so it will no longer receive events.
     *
     * @param handler Function pointer previously passed to RegisterHandler().
     *
     * @note Safe to call even if the handler is not currently registered.
     */
    void UnregisterHandler(KeyHandler handler);

}


