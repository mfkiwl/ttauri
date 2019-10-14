// Copyright 2019 Pokitec
// All rights reserved.

#pragma once

#include "TTauri/Foundation/URL.hpp"

namespace TTauri::Config {

struct ASTObject;

ASTObject *parseConfigFile(URL const &path);

}

