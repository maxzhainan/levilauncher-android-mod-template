#pragma once

#include <string>

#include "pl/cpp/Config.hpp"

namespace pl::mod {
class NativeMod;
}

namespace my_mod {

struct ModConfig {
    int version = 1;
    bool enabled = true;
    std::string message = "Hello from My Mod";
};

nlohmann::json makeDefaultConfigJson();
nlohmann::json makeConfigSchemaJson();

class MyMod {
  public:
    static MyMod &getInstance();

    [[nodiscard]] pl::mod::NativeMod &getSelf() const;

    bool load();
    bool enable();
    bool disable();
    bool unload();

  private:
    ModConfig config;
};

} // namespace my_mod

template <> struct pl::config::Schema<my_mod::ModConfig> {
    static constexpr std::string_view title = "My Mod Config";
    static constexpr std::string_view description = {};

    static constexpr FieldSchema field(std::string_view name) {
        if (name == "version")
            return {.title = "Version", .readOnly = true};
        if (name == "enabled")
            return {.title = "Enabled", .description = "Turns the test mod behavior on or off."};
        if (name == "message")
            return {.title = "Message", .description = "Message written when the mod is enabled."};
        return {};
    }
};
