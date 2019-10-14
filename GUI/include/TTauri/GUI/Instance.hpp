// Copyright 2019 Pokitec
// All rights reserved.

#pragma once

#include "TTauri/Foundation/os_detect.hpp"

#if OPERATING_SYSTEM == OS_WINDOWS
#include "TTauri/GUI/Instance_vulkan_win32.hpp"
namespace TTauri::GUI {
using Instance = Instance_vulkan_win32;
}

#elif OPERATING_SYSTEM == OS_MACOS
#include "TTauri/GUI/Instance_vulkan_macos.hpp"
namespace TTauri::GUI {
using Instance = Instance_vulkan_macos;
}

#else
#error "Instance not implemented for os"
#endif

