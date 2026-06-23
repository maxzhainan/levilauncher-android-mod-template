#include "mod/MyMod.h"

#include <filesystem>

#include "pl/cpp/Config.hpp"
#include "pl/cpp/Mod.hpp"

namespace my_mod {

MyMod &MyMod::getInstance() {
    static MyMod instance;
    return instance;
}

pl::mod::NativeMod &MyMod::getSelf() const {
    return *pl::mod::NativeMod::current();
}

bool MyMod::load() {
    auto &self = getSelf();
    self.getLogger().debug("Loading...");

    std::error_code ec;
    std::filesystem::create_directories(self.getDataDir(), ec);
    if (ec) {
        self.getLogger().error("Failed to create data directory {}: {}", self.getDataDir().string(),
                               ec.message());
        return false;
    }

    std::filesystem::create_directories(self.getConfigDir(), ec);
    if (ec) {
        self.getLogger().error("Failed to create config directory {}: {}",
                               self.getConfigDir().string(), ec.message());
        return false;
    }

    pl::config::ConfigFile<ModConfig> configFile;
    if (!configFile.load()) {
        self.getLogger().warn("Failed to load typed config");
        return false;
    }
    config = configFile.value();

    self.getLogger().info("Loaded {} from {}", self.getName(), self.getModDir().string());
    return true;
}

bool MyMod::enable() {
    getSelf().getLogger().debug("Enabling...");
    if (!config.enabled) {
        getSelf().getLogger().info("Test mod is disabled by config");
        return true;
    }

    getSelf().getLogger().info("Config message: {}", config.message);
    return true;
}

bool MyMod::disable() {
    getSelf().getLogger().debug("Disabling...");
    // Undo enable-time state here.
    return true;
}

bool MyMod::unload() {
    getSelf().getLogger().debug("Unloading...");
    // Release load-time resources here.
    return true;
}

} // namespace my_mod
