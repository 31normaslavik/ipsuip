#pragma once

/** Платформенные пути: кэш и конфиг для ipsuip. **/

#include <filesystem>
#include <string>

namespace ipsuip::paths {

// Домашний каталог пользователя.
std::filesystem::path home_dir();

// Кэш: $XDG_CACHE_HOME/ipsuip/data.json (Linux),
// %LOCALAPPDATA%/ipsuip/data.json (Windows).
std::filesystem::path cache_dir();

// Конфиг: $XDG_CONFIG_HOME/ipsuip (Linux),
// %APPDATA%/ipsuip (Windows).
std::filesystem::path config_dir();

} // namespace ipsuip::paths
