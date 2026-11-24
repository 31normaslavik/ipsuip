#pragma once

/** Конфигурация ipsuip: строка прокси. **/

#include <filesystem>
#include <string>

namespace ipsuip {

// Путь к файлу конфига: $XDG_CONFIG_HOME/ipsuip/config.json (Linux),
// %APPDATA%/ipsuip/config.json (Windows).
std::filesystem::path default_config_path();

// Единственная опция конфига.
struct Config {
  std::string proxy; // "[scheme://][user:pass@]host:port", пусто — без прокси
};

// Загружает конфиг. Если файла нет — возвращает Config с пустым прокси.
Config load_config();

// Сохраняет конфиг на диск. Возвращает false при ошибке.
bool save_config(const Config &cfg);

} // namespace ipsuip
