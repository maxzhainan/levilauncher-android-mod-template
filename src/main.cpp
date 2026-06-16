#include "mod/MyMod.h"

#include "pl/cpp/mod/RegisterHelper.hpp"

PL_REGISTER_MOD(my_mod::MyMod, my_mod::MyMod::getInstance());
