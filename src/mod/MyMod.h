#pragma once

#include <string>
#include <vector>

#include "pl/cpp/Config.hpp"

namespace pl::mod {
class NativeMod;
}

namespace my_mod {

struct HudConfig {
    bool showMessage = true;
    std::string message = "Levi config reflection test";
    double scale = 1.25;
};

struct LimitsConfig {
    int maxEvents = 8;
    double cooldownSeconds = 2.5;
};

struct FeatureConfig {
    std::string name = "new-feature";
    bool enabled = true;
    int weight = 1;
};

enum class Profile {
    Quiet,
    Balanced,
    Verbose,
};

struct ModConfig {
    int version = 1;
    bool enabled = true;
    Profile profile = Profile::Balanced;
    HudConfig hud;
    LimitsConfig limits;
    std::vector<FeatureConfig> features = {
        {"logger", true, 1},
        {"nested-editor", true, 2},
    };
    std::vector<std::string> tags = {"config", "reflection", "launcher-ui"};
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

    void logConfigSummary() const;
};

} // namespace my_mod

template <> struct pl::config::Schema<my_mod::HudConfig> {
    static constexpr FieldSchema field(std::string_view name) {
        if (name == "showMessage")
            return {.title = "Show Message"};
        if (name == "message")
            return {.title = "Message"};
        if (name == "scale")
            return {.title = "Scale", .minimum = 0.5, .maximum = 3.0};
        return {};
    }
};

template <> struct pl::config::Schema<my_mod::LimitsConfig> {
    static constexpr FieldSchema field(std::string_view name) {
        if (name == "maxEvents")
            return {.title = "Max Events", .minimum = 1, .maximum = 64};
        if (name == "cooldownSeconds")
            return {.title = "Cooldown Seconds", .minimum = 0, .maximum = 30};
        return {};
    }
};

template <> struct pl::config::Schema<my_mod::FeatureConfig> {
    static constexpr FieldSchema field(std::string_view name) {
        if (name == "name")
            return {.title = "Name"};
        if (name == "enabled")
            return {.title = "Enabled"};
        if (name == "weight")
            return {.title = "Weight", .minimum = 0, .maximum = 10};
        return {};
    }
};

template <> struct pl::config::Schema<my_mod::ModConfig> {
    static constexpr std::string_view title = "Test Mod Config";
    static constexpr std::string_view description =
        "Covers nested objects, arrays, enums, booleans, strings and numeric ranges.";

    static constexpr FieldSchema field(std::string_view name) {
        if (name == "version")
            return {.title = "Version", .readOnly = true};
        if (name == "enabled")
            return {.title = "Enabled", .description = "Turns the test mod behavior on or off."};
        if (name == "profile")
            return {.title = "Profile", .description = "Enum field used to verify spinner rendering."};
        if (name == "hud")
            return {.title = "HUD", .description = "Nested object used to verify expandable groups."};
        if (name == "limits")
            return {.title = "Limits"};
        if (name == "features")
            return {.title = "Features",
                    .description = "Array of objects used to verify add/remove/reorder and nested element editing."};
        if (name == "tags")
            return {.title = "Tags", .description = "Array of strings used to verify primitive array editing."};
        return {};
    }
};
