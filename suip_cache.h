#pragma once

/** JSON-кэш навигационных страниц suip.biz (списки целей иерархии). **/

#include "suip_model.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace ipsuip {

// Путь к файлу кэша: $XDG_CONFIG_HOME/suipbiz/data.json,
// иначе ~/.config/suipbiz/data.json.
std::filesystem::path default_cache_path();

// Кэш навигационных страниц: URL -> список целей.
// Страницы с IP-диапазонами здесь не хранятся никогда: они запрашиваются заново.
class NavCache {
public:
  NavCache(); // загружает кэш по default_cache_path()
  explicit NavCache(std::filesystem::path path);

  // Возвращает цели страницы или nullptr, если её нет в кэше.
  const std::vector<Target> *find(const std::string &url) const;

  // Запоминает цели страницы. Пустой список (страница с IP-адресами) и пустой
  // URL игнорируются. Запись на диск выполняет save().
  void store(const std::string &url, const std::vector<Target> &targets);

  // Перечитывает/записывает файл. Возвращают false, если файла нет (load)
  // или его не удалось записать (save).
  bool load();
  bool save() const;

  const std::filesystem::path &path() const { return path_; }
  size_t size() const { return pages_.size(); }

private:
  std::filesystem::path path_;
  std::unordered_map<std::string, std::vector<Target>> pages_;
};

} // namespace ipsuip
