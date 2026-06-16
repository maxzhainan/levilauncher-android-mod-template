#include "mod/MyMod.h"

#include <filesystem>
#include <iostream>

namespace {

int failures = 0;

void expect(bool condition, const char *message) {
    if (condition)
        return;
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

void testDefaultJson() {
    auto json = my_mod::makeDefaultConfigJson();
    expect(json["version"] == 1, "default version");
    expect(json["enabled"] == true, "default enabled");
    expect(json["profile"] == "Balanced", "enum serializes as string");
    expect(json["hud"]["message"] == "Levi config reflection test", "nested message");
    expect(json["features"].is_array() && json["features"].size() == 2, "features array");
    expect(json["tags"].is_array() && json["tags"].size() == 3, "tags array");
}

void testMergeAndRewrite() {
    auto root = std::filesystem::temp_directory_path() / "levi_config_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    auto configPath = root / "config.json";
    auto schemaPath = root / "config.schema.json";
    pl::config::writeConfig(configPath, nlohmann::json{
        {"version", 0},
        {"enabled", false},
        {"profile", "Verbose"},
        {"hud", {{"message", "custom"}}},
    });

    pl::config::ConfigFile<my_mod::ModConfig> file(my_mod::ModConfig{}, configPath, schemaPath);
    expect(file.load(), "typed config loads");
    expect(file.value().enabled == false, "user bool preserved");
    expect(file.value().profile == my_mod::Profile::Verbose, "user enum preserved");
    expect(file.value().hud.message == "custom", "user nested string preserved");
    expect(file.value().hud.showMessage == true, "missing nested bool defaulted");
    expect(file.value().limits.maxEvents == 8, "missing object defaulted");
    expect(std::filesystem::exists(schemaPath), "schema written");

    auto rewritten = pl::config::loadConfig(configPath, nlohmann::json::object());
    expect(rewritten["version"] == 1, "version rewritten");
    expect(rewritten["hud"].contains("scale"), "normalized nested field written");
    expect(rewritten.contains("features"), "normalized array field written");

    std::filesystem::remove_all(root);
}

void testSchema() {
    auto schema = my_mod::makeConfigSchemaJson();
    expect(schema["title"] == "Test Mod Config", "schema title");
    expect(schema["properties"]["version"]["readOnly"] == true, "readonly metadata");
    expect(schema["properties"]["profile"]["enum"].size() == 3, "enum schema");
    expect(schema["properties"]["hud"]["properties"]["scale"]["minimum"] == 0.5, "minimum metadata");
    expect(schema["properties"]["limits"]["properties"]["maxEvents"]["maximum"] == 64, "maximum metadata");
    expect(schema["properties"]["features"]["items"]["properties"]["weight"]["default"] == 1, "array item default");
    expect(schema["properties"]["tags"]["items"]["type"] == "string", "primitive array item schema");
}

} // namespace

int main() {
    testDefaultJson();
    testMergeAndRewrite();
    testSchema();
    if (failures != 0)
        return 1;
    std::cout << "All config tests passed\n";
    return 0;
}
