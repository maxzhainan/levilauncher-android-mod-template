#include "mod/MyMod.h"

namespace my_mod {

nlohmann::json makeDefaultConfigJson() {
    return pl::config::defaultJson(ModConfig{});
}

nlohmann::json makeConfigSchemaJson() {
    return pl::config::schema(ModConfig{});
}

} // namespace my_mod
