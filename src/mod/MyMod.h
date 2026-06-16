#pragma once

#include <memory>

#include "pl/cpp/Mod.hpp"

namespace my_mod {

class MyMod {
  public:
    static MyMod &getInstance();

    [[nodiscard]] pl::mod::NativeMod &getSelf() const;

    bool load();
    bool enable();
    bool disable();
    bool unload();
};

} // namespace my_mod
