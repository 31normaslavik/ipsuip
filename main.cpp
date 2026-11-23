/** Made by I'm Pancake **/

// #include "choose_your_fate.h"
#include "http_client_sync_ssl.h"
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <lexbor/html/html.h>
#include <lexbor/html/interfaces/document.h>
#include <lexbor/url/url.h>
#include <string>
#include <lexbor/html/html.h>
#include <lexbor/dom/dom.h>
#include <lexbor/html/serialize.h>
#include <cctype>
#include <cstring>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <sstream>

// ---- модель ------------------------------------------------------------

enum class TargetLevel { Continent, Country, Province, City };

static const char *level_name(TargetLevel level) {
  switch (level) {
    case TargetLevel::Continent: return "continent";
    case TargetLevel::Country:   return "country";
    case TargetLevel::Province:  return "province";
    case TargetLevel::City:      return "city";
  }
  return "unknown";
}

struct Target {
  TargetLevel level = TargetLevel::Continent;
  std::string code;                   // значение параметра уровня: EU, BY, Brest ...
  std::string location;               // отображаемое название
  std::string address;                // абсолютный URL страницы уровня
  std::string all_ip_direct_download; // абсолютный URL выгрузки всех IP (может быть пустым)
};

// ---- вспомогательные --------------------------------------------------

static std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::string node_text(lxb_dom_node_t* node) {
    std::string out;
    std::vector<lxb_dom_node_t*> stack{node};
    while (!stack.empty()) {
        lxb_dom_node_t* n = stack.back();
        stack.pop_back();
        if (n->type == LXB_DOM_NODE_TYPE_TEXT) {
            auto* t = lxb_dom_interface_text(n);
            out.append(reinterpret_cast<const char*>(t->char_data.data.data),
                       t->char_data.data.length);
        } else {
            for (lxb_dom_node_t* c = n->first_child; c; c = c->next)
                stack.push_back(c);
        }
    }
    return out;
}

static std::string element_attr(lxb_dom_element_t* el, const char* name) {
    size_t len = 0;
    const lxb_char_t* v = lxb_dom_element_get_attribute(
        el,
        reinterpret_cast<const lxb_char_t*>(name),
        std::strlen(name),
        &len);
    return v ? std::string(reinterpret_cast<const char*>(v), len) : std::string();
}

static void collect_anchors(lxb_dom_node_t* node,
                            std::vector<lxb_dom_element_t*>& out) {
    for (lxb_dom_node_t* c = node->first_child; c; c = c->next) {
        if (c->type == LXB_DOM_NODE_TYPE_ELEMENT) {
            auto* el = lxb_dom_interface_element(c);
            size_t len = 0;
            const lxb_char_t* tag = lxb_dom_element_local_name(el, &len);
            if (tag && len == 1 && tag[0] == 'a') out.push_back(el);
            collect_anchors(c, out);
        }
    }
}

// ---- вспомогательные для разбора ссылок --------------------------------

static bool starts_with(const std::string& s, const std::string& prefix) {
  return s.rfind(prefix, 0) == 0;
}

static bool ends_with(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// lexbor может отдавать href с HTML-сущностями (&amp;) — приводим к обычному виду
static std::string normalize_href(const std::string& href) {
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
static std::string query_only(const std::string& href) {
  size_t q = href.find('?');
  return q == std::string::npos ? std::string() : href.substr(q);
}

static bool has_param(const std::string& query, const char* key) {
  const std::string k(key);
  return query.find("&" + k + "=") != std::string::npos ||
         starts_with(query, "?" + k + "=");
}

static std::string param_value(const std::string& query, const char* key) {
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
static std::string url_decode(const std::string& s) {
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
static bool level_of_query(const std::string& query, TargetLevel& level,
                           const char*& key) {
  static const struct {
    const char* name;
    TargetLevel level;
  } kLevels[] = {
      {"city", TargetLevel::City},
      {"province", TargetLevel::Province},
      {"country", TargetLevel::Country},
      {"continent", TargetLevel::Continent},
  };

  for (const auto& l : kLevels) {
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
static std::string location_from_anchor(lxb_dom_element_t* el) {
  auto* node = reinterpret_cast<lxb_dom_node_t*>(el);

  std::string before = node->prev ? trim(node_text(node->prev)) : std::string();
  std::string after  = node->next ? trim(node_text(node->next)) : std::string();

  if (before.find("Browse") != std::string::npos ||
      after.rfind("city (town)", 0) == 0)
    return trim(node_text(node));

  return {};
}

// ---- основная функция --------------------------------------------------

// Разбирает страницу любого уровня и собирает все найденные цели.
// Ссылки "?act=all-country-ip&..." дают address, парные "&all"/"&all-download"
// дают all_ip_direct_download.
std::vector<Target> parse_targets(const std::string& html,
                                  const std::string& base_url = "https://suip.biz/") {
  std::vector<TargetSlot> slots;
  std::unordered_map<std::string, size_t> index; // query без &all -> индекс в slots

  lxb_html_document_t* doc = lxb_html_document_create();
  if (!doc) return {};

  lxb_status_t st = lxb_html_document_parse(
      doc, reinterpret_cast<const lxb_char_t*>(html.data()), html.size());
  if (st != LXB_STATUS_OK) {
    lxb_html_document_destroy(doc);
    return {};
  }

  std::vector<lxb_dom_element_t*> anchors;
  collect_anchors(reinterpret_cast<lxb_dom_node_t*>(doc), anchors);

  for (auto* a : anchors) {
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
    const char* key = nullptr;
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

    TargetSlot& slot = slots[it->second];

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
  for (auto& slot : slots) targets.push_back(std::move(slot.target));
  return targets;
}

// ---- сеть --------------------------------------------------------------

// Скачивает страницу и сразу разбирает её (boost/beast + lexbor).
std::vector<Target> fetch_targets(const std::string& page_url) {
  const std::string scheme = "https://";
  if (!starts_with(page_url, scheme))
    throw std::runtime_error("Only https:// URLs are supported: " + page_url);

  size_t host_end = page_url.find('/', scheme.size());
  std::string host = page_url.substr(
      scheme.size(),
      host_end == std::string::npos ? std::string::npos : host_end - scheme.size());
  if (host.empty())
    throw std::runtime_error("Cannot extract host from URL: " + page_url);

  std::string target = host_end == std::string::npos ? "/" : page_url.substr(host_end);
  const std::string html = ipsuip::get(host, target);
  
  if(!html.empty()){
    std::filesystem::path path_html{"last_html.html"};
    std::ofstream ofs(path_html);
    if (!ofs)
      throw std::runtime_error("File creation error: " + path_html.string());
    else {
      std::istringstream ss{html};
      for(std::string line; std::getline(ss, line);){
        ofs << line << "\n";
      }
   }
    ofs.close();
  }
  
  return parse_targets(html, scheme + host + "/");
}



void save_to_file(const std::string &data_raw_html,
                  const std::string &file_name) {
  if (data_raw_html.empty())
    throw std::runtime_error("data html is empty!");

  std::ofstream file;
  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    file.open(file_name);
    if (!file)
      throw std::runtime_error("File creation error: " + file_name);

    file << data_raw_html << std::endl;
    file.close();

    std::filesystem::path path{file_name};
    if (std::filesystem::exists(path))
      std::cout << "Size of " + file_name + " "
                << std::filesystem::file_size(path) << "\n";
  } catch (std::ofstream::failure &e) {
    std::cerr << e.what() << " code: " << e.code() << std::endl;
  }
}

static lxb_status_t callback(const lxb_char_t *data, size_t len, void *ctx) {
  printf("%.*s", (int)len, (const char *)data);

  return LXB_STATUS_OK;
}

int lexbor_test(const std::string &str) {
  lxb_status_t status;
  lxb_html_document_t *document;
  lxb_dom_collection_t *collection;
  lxb_dom_element_t *element;
  lxb_url_parser_t url_parser;
  lxb_url_t *base_url, *url;
  const lxb_char_t *href;
  size_t href_len;

  static const lxb_char_t *html = reinterpret_cast<const lxb_char_t*>(str.c_str());
    //   "<html><head><title>Example</title></head>"
    //   "<body>"
    //   "<a href=\"/about\">About</a>"
    //   "<a href=\"news/today.html\">Today's News</a>"
    //   "<a href=\"https://other.com/page\">Other Site</a>"
    //   "<a href=\"../archive/2024.html\">Archive</a>"
    //   "<a>No href</a>"
    //   "</body></html>";
  size_t html_len = str.length();

  static const lxb_char_t base_url_str[] =
      "https://suip.biz/";
  size_t base_url_len = sizeof(base_url_str) - 1;

  /* Step 1: Parse the HTML document. */

  document = lxb_html_document_create();
  if (document == NULL) {
    std::cerr << "document == NULL" << std::endl;
  }

  status = lxb_html_document_parse(document, html, html_len);
  if (status != LXB_STATUS_OK) {
    std::cerr << "status != LXB_STATUS_OK" << std::endl;
  }
  /* Step 2: Find all <a> elements. */

  collection = lxb_dom_collection_make(&document->dom_document, 128);
  if (collection == NULL) {
    std::cerr << "collection == NULL" << std::endl;
  }


  status =
      lxb_dom_elements_by_tag_name(lxb_dom_interface_element(document->body),
                                   collection, (const lxb_char_t *)"a", 1);
  if (status != LXB_STATUS_OK) {
    std::cerr << "status != LXB_STATUS_OK" << std::endl;
  }

  printf("Found %zu link(s).\n\n", lxb_dom_collection_length(collection));

  /* Step 3: Initialize the URL parser and parse the base URL. */

  status = lxb_url_parser_init(&url_parser, NULL);
  if (status != LXB_STATUS_OK) {
    std::cerr << "status != LXB_STATUS_OK" << std::endl;
  }

  base_url = lxb_url_parse(&url_parser, NULL, base_url_str, base_url_len);
  if (base_url == NULL) {
    std::cerr << "base_url == NULL" << std::endl;
  }

  /* Step 4: Iterate links, extract href, and resolve each URL. */

  for (size_t i = 0; i < lxb_dom_collection_length(collection); i++) {
    element = lxb_dom_collection_element(collection, i);

    href = lxb_dom_element_get_attribute(element, (const lxb_char_t *)"href", 4,
                                         &href_len);
    if (href == NULL) {
      printf("[%zu] <a> without href, skipping.\n", i);
      continue;
    }

    printf("[%zu] href: %.*s\n", i, (int)href_len, (const char *)href);

    /* Resolve the href against the base URL. */

    lxb_url_parser_clean(&url_parser);

    url = lxb_url_parse(&url_parser, base_url, href, href_len);
    if (url == NULL) {
      printf("     Failed to parse URL.\n");
      continue;
    }

    printf("     Resolved: ");
    (void)lxb_url_serialize(url, callback, NULL, false);
    printf("\n");
  }

  /* Cleanup. */

  lxb_dom_collection_destroy(collection, true);
  lxb_url_parser_destroy(&url_parser, false);
  lxb_url_memory_destroy(base_url);
  lxb_html_document_destroy(document);

  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  try {
    std::vector<Target> targets;

    if (argc == 2) {
      // Режим сети: любая страница иерархии, например
      // ./ipsuip_develop "https://suip.biz/?act=all-country-ip&country=BY"
      targets = fetch_targets(argv[1]);
    } else {
      // Оффлайн-режим: разбор локального файла из текущего каталога.
      std::cout << std::filesystem::current_path() << "\n";
      std::filesystem::path path("CountriesEurope.html");

      if (!std::filesystem::exists(path))
        throw std::runtime_error("File not found: " + path.string());

      std::ifstream ifs(path);
      if (!ifs)
        throw std::runtime_error("File open error: " + path.filename().string());

      std::string all_file;
      for (std::string temp_f; std::getline(ifs, temp_f);) {
        all_file += temp_f;
        all_file.append("\n");
      }

      targets = parse_targets(all_file);
    }

    for (const auto& t : targets) {
      std::cout << "[" << level_name(t.level) << "]"
                << " CODE=" << t.code
                << " | Location=" << t.location
                << " | Adress=" << t.address
                << " | ALL_IP_Direct_Download=" << t.all_ip_direct_download << "\n";
    }

  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}
