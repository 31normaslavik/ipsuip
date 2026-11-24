#include "suip_model.h"

#include "http_client_sync_ssl.h"

#include <lexbor/dom/dom.h>
#include <lexbor/html/html.h>

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

// ---- вспомогательные --------------------------------------------------

static std::string trim(const std::string &s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

static std::string node_text(lxb_dom_node_t *node) {
  std::string out;
  std::vector<lxb_dom_node_t *> stack{node};
  while (!stack.empty()) {
    lxb_dom_node_t *n = stack.back();
    stack.pop_back();
    if (n->type == LXB_DOM_NODE_TYPE_TEXT) {
      auto *t = lxb_dom_interface_text(n);
      out.append(reinterpret_cast<const char *>(t->char_data.data.data),
                 t->char_data.data.length);
    } else {
      for (lxb_dom_node_t *c = n->first_child; c; c = c->next)
        stack.push_back(c);
    }
  }
  return out;
}

static std::string element_attr(lxb_dom_element_t *el, const char *name) {
  size_t len = 0;
  const lxb_char_t *v = lxb_dom_element_get_attribute(
      el, reinterpret_cast<const lxb_char_t *>(name), std::strlen(name), &len);
  return v ? std::string(reinterpret_cast<const char *>(v), len)
           : std::string();
}

static void collect_anchors(lxb_dom_node_t *node,
                            std::vector<lxb_dom_element_t *> &out) {
  for (lxb_dom_node_t *c = node->first_child; c; c = c->next) {
    if (c->type == LXB_DOM_NODE_TYPE_ELEMENT) {
      auto *el = lxb_dom_interface_element(c);
      size_t len = 0;
      const lxb_char_t *tag = lxb_dom_element_local_name(el, &len);
      if (tag && len == 1 && tag[0] == 'a') out.push_back(el);
      collect_anchors(c, out);
    }
  }
}

// ---- вспомогательные для разбора ссылок --------------------------------

static bool starts_with(const std::string &s, const std::string &prefix) {
  return s.rfind(prefix, 0) == 0;
}

static bool ends_with(const std::string &s, const std::string &suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// lexbor может отдавать href с HTML-сущностями (&amp;) — приводим к обычному виду
static std::string normalize_href(const std::string &href) {
  std::string out;
  out.reserve(href.size());
  for (size_t i = 0; i < href.size();) {
    if (href.compare(i, 5, "&amp;") == 0) {
      out += '&';
      i += 5;
    } else if (href.compare(i, 4, "&#38;") == 0) {
      out += '&';
      i += 4;
    } else {
      out += href[i++];
    }
  }
  return out;
}

// отбрасывает схему, хост и путь, оставляя только query-строку
static std::string query_only(const std::string &href) {
  size_t q = href.find('?');
  return q == std::string::npos ? std::string() : href.substr(q);
}

static bool has_param(const std::string &query, const char *key) {
  const std::string k(key);
  return query.find("&" + k + "=") != std::string::npos ||
         starts_with(query, "?" + k + "=");
}

static std::string param_value(const std::string &query, const char *key) {
  const std::string k(key);
  size_t start;
  size_t pos = query.find("&" + k + "=");
  if (pos != std::string::npos)
    start = pos + k.size() + 2;
  else if (starts_with(query, "?" + k + "="))
    start = k.size() + 2;
  else
    return {};

  size_t end = query.find('&', start);
  return query.substr(start,
                      end == std::string::npos ? std::string::npos : end - start);
}

// %20 -> ' ', %28 -> '(' и т.п.
static std::string url_decode(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size() &&
        std::isxdigit(static_cast<unsigned char>(s[i + 1])) &&
        std::isxdigit(static_cast<unsigned char>(s[i + 2]))) {
      out += static_cast<char>(std::stoi(s.substr(i + 1, 2), nullptr, 16));
      i += 2;
    } else if (s[i] == '+') {
      out += ' ';
    } else {
      out += s[i];
    }
  }
  return out;
}

// Уровень определяется самым «глубоким» параметром query-строки, поэтому
// одна и та же функция разбирает страницы всех уровней:
//   ?act=all-country-ip                                  — корень (пропускается)
//   ?act=all-country-ip&continent=EU                     — континент
//   ?act=all-country-ip&country=BY                       — страна
//   ?act=all-country-ip&country=BY&province=Brest        — регион/провинция
//   ?act=all-country-ip&country=BY&province=Brest&city=Brest — город
static bool level_of_query(const std::string &query, TargetLevel &level,
                           const char *&key) {
  static const struct {
    const char *name;
    TargetLevel level;
  } kLevels[] = {
      {"city", TargetLevel::City},
      {"province", TargetLevel::Province},
      {"country", TargetLevel::Country},
      {"continent", TargetLevel::Continent},
  };

  for (const auto &l : kLevels) {
    if (has_param(query, l.name)) {
      level = l.level;
      key = l.name;
      return true;
    }
  }
  return false;
}

struct TargetSlot {
  Target target;
  int download_rank = 0; // 1 — &all-download, 2 — &all (предпочтительнее)
};

// Название локации берём только из «содержательных» ссылок suip.biz:
//   <p>Browse <a>Europe</a> (or get ...)
//   <p><a>Brest</a> city (town) all IP addresses</p>
// Служебные ссылки вроде "(FOUR LEVELS UP)" названием не считаются.
static std::string location_from_anchor(lxb_dom_element_t *el) {
  auto *node = reinterpret_cast<lxb_dom_node_t *>(el);

  std::string before = node->prev ? trim(node_text(node->prev)) : std::string();
  std::string after = node->next ? trim(node_text(node->next)) : std::string();

  if (before.find("Browse") != std::string::npos ||
      after.rfind("city (town)", 0) == 0)
    return trim(node_text(node));

  return {};
}

// ---- разбор списка IP-диапазонов ---------------------------------------

static bool is_ipv4(const std::string &s) {
  int parts = 0;
  size_t start = 0;
  while (start <= s.size()) {
    size_t end = s.find('.', start);
    if (end == std::string::npos) end = s.size();

    std::string part = s.substr(start, end - start);
    if (part.empty() || part.size() > 3) return false;
    for (char c : part)
      if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    if (std::stoi(part) > 255) return false;

    ++parts;
    if (end == s.size()) break;
    start = end + 1;
  }
  return parts == 4;
}

static bool is_ipv6(const std::string &s) {
  if (s.find(':') == std::string::npos) return false;
  for (char c : s)
    if (!std::isxdigit(static_cast<unsigned char>(c)) && c != ':') return false;

  size_t double_colon = s.find("::");
  if (double_colon != std::string::npos &&
      s.find("::", double_colon + 1) != std::string::npos)
    return false;

  int groups = 0;
  size_t start = 0;
  while (start <= s.size()) {
    size_t end = s.find(':', start);
    if (end == std::string::npos) end = s.size();

    std::string group = s.substr(start, end - start);
    if (group.size() > 4) return false;
    if (!group.empty()) ++groups;

    if (end == s.size()) break;
    start = end + 1;
  }

  return double_colon != std::string::npos ? groups <= 7 : groups == 8;
}

// Строка вида "192.168.1.0-192.168.255.255" или
// "2a03:d000:8000::-2a03:d000:8000:ffff:ffff:ffff:ffff:ffff".
static bool is_ip_range(const std::string &s) {
  size_t dash = s.find('-');
  if (dash == std::string::npos || s.find('-', dash + 1) != std::string::npos)
    return false;

  std::string first = s.substr(0, dash);
  std::string last = s.substr(dash + 1);
  return (is_ipv4(first) && is_ipv4(last)) || (is_ipv6(first) && is_ipv6(last));
}

static void collect_elements_by_tag(lxb_dom_node_t *node, const char *name,
                                    std::vector<lxb_dom_element_t *> &out) {
  const size_t name_len = std::strlen(name);
  for (lxb_dom_node_t *c = node->first_child; c; c = c->next) {
    if (c->type != LXB_DOM_NODE_TYPE_ELEMENT) continue;

    auto *el = lxb_dom_interface_element(c);
    size_t len = 0;
    const lxb_char_t *tag = lxb_dom_element_local_name(el, &len);
    if (tag && len == name_len &&
        std::strncmp(reinterpret_cast<const char *>(tag), name, name_len) == 0)
      out.push_back(el);

    collect_elements_by_tag(c, name, out);
  }
}

// ---- сеть --------------------------------------------------------------

// Скачивает произвольный https-URL и возвращает тело ответа.
static std::string http_get_url(const std::string &page_url,
                                const ipsuip::ProgressCallback &progress) {
  ipsuip::GetOptions opts;
  opts.progress = progress;
  return ipsuip::get(page_url, opts);
}

// ---- публичный API -----------------------------------------------------

// Разбирает страницу любого уровня и собирает все найденные цели.
// Ссылки "?act=all-country-ip&..." дают address, парные "&all"/"&all-download"
// дают all_ip_direct_download.
std::vector<Target> parse_targets(const std::string &html,
                                  const std::string &base_url) {
  std::vector<TargetSlot> slots;
  std::unordered_map<std::string, size_t> index; // query без &all -> индекс

  lxb_html_document_t *doc = lxb_html_document_create();
  if (!doc) return {};

  lxb_status_t st = lxb_html_document_parse(
      doc, reinterpret_cast<const lxb_char_t *>(html.data()), html.size());
  if (st != LXB_STATUS_OK) {
    lxb_html_document_destroy(doc);
    return {};
  }

  std::vector<lxb_dom_element_t *> anchors;
  collect_anchors(reinterpret_cast<lxb_dom_node_t *>(doc), anchors);

  for (auto *a : anchors) {
    std::string href = normalize_href(element_attr(a, "href"));
    if (href.empty()) continue;

    std::string query = query_only(href);
    if (!starts_with(query, "?act=all-country-ip")) continue;

    // Отделяем «скачать всё» от обычной навигации по иерархии.
    std::string base = query;
    bool is_all = false;
    bool is_download = false;
    if (ends_with(base, "&all-download")) {
      base.resize(base.size() - std::strlen("&all-download"));
      is_download = true;
    } else if (ends_with(base, "&all")) {
      base.resize(base.size() - std::strlen("&all"));
      is_all = true;
    }

    TargetLevel level = TargetLevel::Continent;
    const char *key = nullptr;
    // Корневая ссылка "?act=all-country-ip" уровня не содержит — пропускаем.
    if (!level_of_query(base, level, key)) continue;

    std::string code = url_decode(param_value(base, key));
    if (code.empty()) continue;

    auto it = index.find(base);
    if (it == index.end()) {
      TargetSlot slot;
      slot.target.level = level;
      slot.target.code = code;
      slot.target.address = base_url + base;
      slots.push_back(std::move(slot));
      it = index.emplace(base, slots.size() - 1).first;
    }

    TargetSlot &slot = slots[it->second];

    if (!is_all && !is_download) {
      std::string text = location_from_anchor(a);
      if (!text.empty()) slot.target.location = text;
      continue;
    }

    int rank = is_all ? 2 : 1;
    if (rank > slot.download_rank) {
      slot.download_rank = rank;
      slot.target.all_ip_direct_download =
          starts_with(href, "http") ? href : base_url + href;
    }
  }

  lxb_html_document_destroy(doc);

  std::vector<Target> targets;
  targets.reserve(slots.size());
  for (auto &slot : slots) targets.push_back(std::move(slot.target));
  return targets;
}

// Страница с IP-адресами содержит <pre>, в котором каждая строка — диапазон.
// Возвращает только валидные диапазоны, без тегов и служебного текста.
std::vector<std::string> parse_ip_ranges(const std::string &html) {
  std::vector<std::string> ranges;

  lxb_html_document_t *doc = lxb_html_document_create();
  if (!doc) return ranges;

  lxb_status_t st = lxb_html_document_parse(
      doc, reinterpret_cast<const lxb_char_t *>(html.data()), html.size());
  if (st != LXB_STATUS_OK) {
    lxb_html_document_destroy(doc);
    return ranges;
  }

  std::vector<lxb_dom_element_t *> pres;
  collect_elements_by_tag(reinterpret_cast<lxb_dom_node_t *>(doc), "pre", pres);

  for (auto *pre : pres) {
    std::string text = node_text(reinterpret_cast<lxb_dom_node_t *>(pre));
    std::istringstream ss(text);
    for (std::string line; std::getline(ss, line);) {
      line = trim(line);
      if (!line.empty() && is_ip_range(line)) ranges.push_back(std::move(line));
    }
  }

  lxb_html_document_destroy(doc);
  return ranges;
}

// Извлекает диапазоны из <pre>, а если их нет (например, ответ text/plain) —
// построчно просматривает исходный текст, оставляя только валидные диапазоны.
std::vector<std::string> extract_ip_ranges(const std::string &html) {
  std::vector<std::string> ranges = parse_ip_ranges(html);
  if (!ranges.empty()) return ranges;

  std::istringstream ss(html);
  for (std::string line; std::getline(ss, line);) {
    line = trim(line);
    if (!line.empty() && is_ip_range(line)) ranges.push_back(std::move(line));
  }
  return ranges;
}

// Скачивает страницу и разбирает её (boost/beast + lexbor).
PageContent fetch_page(const std::string &page_url,
                       const ipsuip::ProgressCallback &progress) {
  const std::string html = http_get_url(page_url, progress);
  const size_t host_end = page_url.find('/', std::strlen("https://"));
  const std::string base =
      page_url.substr(0, host_end == std::string::npos ? page_url.size()
                                                       : host_end) +
      "/";

  PageContent page;
  page.targets = parse_targets(html, base);
  page.ip_ranges = parse_ip_ranges(html);
  return page;
}

// Скачивает страницу с IP-адресами и возвращает только диапазоны.
// Использует потоковую загрузку во временный файл для больших выгрузок.
std::vector<std::string> fetch_ip_ranges(const std::string &page_url,
                                         const ipsuip::ProgressCallback &progress) {
  // Пытаемся скачать во временный файл
  std::error_code ec;
  std::filesystem::path tmp =
      std::filesystem::temp_directory_path(ec) /
      ("ipsuip_dl_" + std::to_string(std::hash<std::string>{}(page_url)) + ".tmp");
  if (ec) tmp = "ipsuip_dl.tmp";

  try {
    ipsuip::GetOptions opts;
    opts.progress = progress;
    ipsuip::get_to_file(page_url, tmp.string(), opts);

    // Читаем построчно из файла — валидные диапазоны
    std::vector<std::string> ranges;
    std::ifstream ifs(tmp);
    if (ifs) {
      for (std::string line; std::getline(ifs, line);) {
        line = trim(line);
        if (!line.empty() && is_ip_range(line)) ranges.push_back(std::move(line));
      }
      ifs.close();
      std::filesystem::remove(tmp, ec);
      if (!ranges.empty()) return ranges;
    }
    std::filesystem::remove(tmp, ec);
  } catch (...) {
    std::filesystem::remove(tmp, ec);
  }

  // Fallback: обычная загрузка в память
  return extract_ip_ranges(http_get_url(page_url, progress));
}

// URL со всеми IP цели: готовая ссылка выгрузки, иначе конструируется из
// address. Для города парной ссылки "&all" нет — используется страница города.
std::string all_ip_url(const Target &target) {
  if (!target.all_ip_direct_download.empty())
    return target.all_ip_direct_download;
  if (target.address.empty()) return {};
  if (target.level == TargetLevel::City) return target.address;
  return target.address + "&all";
}
