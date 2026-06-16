# LeviLauncher Android Mod Template

This is a CMake template for LeviLauncher Android native mods. It includes a
ready-to-use `manifest.json`, CMake project, sample `MyMod`, packaging script,
and GitHub Actions workflow.

## Project Layout

```text
.
├── CMakeLists.txt
├── manifest.json.in
├── scripts/package.ps1
└── src
    ├── main.cpp
    └── mod
        ├── MyMod.cpp
        └── MyMod.h
```

## Requirements

- Android SDK
- Android NDK 28.2.13676358 or a compatible version
- CMake 3.22+
- Ninja
- PowerShell 7+, or Windows PowerShell

The default `LEVI_PRELOADER_ROOT` is:

```text
D:/a/LiteLDev/LeviLaunchroid/app/src/main/cpp/preloader
```

If your preloader is in another location, set the environment variable or pass
the CMake option directly.

## Build

```powershell
$env:ANDROID_HOME = "C:/Users/<you>/AppData/Local/Android/Sdk"
$env:LEVI_PRELOADER_ROOT = "D:/a/LiteLDev/LeviLaunchroid/app/src/main/cpp/preloader"

./scripts/package.ps1 -Abi arm64-v8a
```

Build both Android architectures:

```powershell
./scripts/package.ps1 -Abi all
```

After the build finishes, import the generated `.levipack` into LeviLauncher.

## Customize Mod Info

Common CMake options:

```powershell
cmake -S . -B build-arm64-v8a `
  -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$env:ANDROID_HOME/ndk/28.2.13676358/build/cmake/android.toolchain.cmake" `
  -DANDROID_ABI=arm64-v8a `
  -DANDROID_PLATFORM=android-28 `
  -DANDROID_STL=c++_shared `
  -DLEVI_PRELOADER_ROOT="$env:LEVI_PRELOADER_ROOT" `
  -DMOD_ID=my-mod `
  -DMOD_NAME="My Mod" `
  -DMOD_AUTHOR="LiteLDev" `
  -DMOD_VERSION=0.1.0 `
  -DMOD_LIBRARY_NAME=my_mod `
  -DMOD_MINECRAFT_VERSIONS='["1.21.*"]' `
  "-DMOD_ICON="

cmake --build build-arm64-v8a --target levi_package
```

## MyMod Lifecycle

Write your mod logic in [src/mod/MyMod.cpp](src/mod/MyMod.cpp):

```cpp
bool MyMod::load() {
    getSelf().getLogger().debug("Loading...");
    return true;
}

bool MyMod::enable() {
    getSelf().getLogger().debug("Enabling...");
    return true;
}

bool MyMod::disable() {
    getSelf().getLogger().debug("Disabling...");
    return true;
}

bool MyMod::unload() {
    getSelf().getLogger().debug("Unloading...");
    return true;
}
```

Lifecycle meaning:

1. `load()` runs when the mod is loaded.
2. `enable()` runs before the game starts.
3. `disable()` runs when the game is closing.
4. `unload()` runs during final mod cleanup.

Common APIs:

- `getLogger()`
- `getId()`
- `getName()`
- `getModDir()`
- `getDataDir()`
- `getConfigDir()`
- `getResourceDir()`
- `getManifestPath()`
- `getLibraryPath()`
- `getJavaVM()`

Use `getDataDir()` and `getConfigDir()` for your mod's own data and config
files.
