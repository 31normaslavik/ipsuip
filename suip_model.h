#pragma once

/** Общая модель данных suip.biz: цель иерархии, содержимое страницы. **/

#include <string>
#include <vector>

enum class TargetLevel { Continent, Country, Province, City };

inline const char *level_name(TargetLevel level) {
  switch (level) {
    case TargetLevel::Continent: return "continent";
    case TargetLevel::Country:   return "country";
    case TargetLevel::Province:  return "province";
    case TargetLevel::City:      return "city";
  }
  return "unknown";
}

// Обратное преобразование "continent"/"country"/... -> TargetLevel.
inline bool level_from_name(const std::string &name, TargetLevel &level) {
  if (name == "continent") level = TargetLevel::Continent;
  else if (name == "country") level = TargetLevel::Country;
  else if (name == "province") level = TargetLevel::Province;
  else if (name == "city") level = TargetLevel::City;
  else return false;
  return true;
}

struct Target {
  TargetLevel level = TargetLevel::Continent;
  std::string code;                   // значение параметра уровня: EU, BY, Brest ...
  std::string location;               // отображаемое название
  std::string address;                // абсолютный URL страницы уровня
  std::string all_ip_direct_download; // абсолютный URL выгрузки всех IP (может быть пустым)
};

// Результат разбора одной страницы: либо навигация по иерархии, либо IP-список.
struct PageContent {
  std::vector<Target> targets;
  std::vector<std::string> ip_ranges;
};

// Разбирает страницу любого уровня и собирает все найденные цели.
std::vector<Target> parse_targets(const std::string &html,
                                  const std::string &base_url = "https://suip.biz/");

// Извлекает IP-диапазоны из <pre> страницы с адресами.
std::vector<std::string> parse_ip_ranges(const std::string &html);

// Извлекает IP-диапазоны из <pre>, а если их нет — построчно из текста
// (например, когда сервер отвечает простым текстом без разметки).
std::vector<std::string> extract_ip_ranges(const std::string &html);

// Скачивает страницу по https-URL и разбирает её.
PageContent fetch_page(const std::string &page_url);

// Скачивает страницу с IP-адресами и возвращает только диапазоны.
std::vector<std::string> fetch_ip_ranges(const std::string &page_url);

// URL со всеми IP цели: готовая ссылка выгрузки, иначе конструируется из
// address. Для города парной ссылки "&all" нет — используется страница города.
std::string all_ip_url(const Target &target);
