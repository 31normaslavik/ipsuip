// SPDX-License-Identifier: MIT
/** Made by I'm Pancake **/

#include "ip_convert.h"
#include "ipsuip_tui.h"
#include "http_client_sync_ssl.h"
#include "config.h"
#include "progress.h"
#include "proxy.h"
#include "suip_cache.h"
#include "suip_model.h"
#include "version.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// ---- модель ------------------------------------------------------------
// enum TargetLevel, struct Target, struct PageContent и объявления
// parse_targets/parse_ip_ranges/fetch_page вынесены в suip_model.h.

// ---- сохранение --------------------------------------------------------

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

// ---- консольный режим --------------------------------------------------

namespace {

constexpr const char *kRootUrl = "https://suip.biz/?act=all-country-ip";

std::string to_lower(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string target_label(const Target &t) {
  return t.location.empty() ? t.code : t.location;
}

// Печатает цели в формате "Название -- КОД", отсортированные по названию.
// Если задан filter_level, печатаются только цели этого уровня.
size_t print_targets(const std::vector<Target> &targets,
                     const TargetLevel *filter_level) {
  std::vector<const Target *> items;
  for (const auto &t : targets)
    if (!filter_level || t.level == *filter_level) items.push_back(&t);

  std::sort(items.begin(), items.end(), [](const Target *a, const Target *b) {
    return to_lower(target_label(*a)) < to_lower(target_label(*b));
  });

  for (const Target *t : items)
    std::cout << target_label(*t) << " -- " << t->code << "\n";
  return items.size();
}

// Ищет цель нужного уровня по коду или названию (без учёта регистра).
const Target *find_target(const std::vector<Target> &targets, TargetLevel level,
                          const std::string &name) {
  const std::string needle = to_lower(name);
  for (const auto &t : targets)
    if (t.level == level && to_lower(t.code) == needle) return &t;
  for (const auto &t : targets)
    if (t.level == level && to_lower(t.location) == needle) return &t;
  return nullptr;
}

struct LoadedPage {
  std::vector<Target> targets;
  std::vector<std::string> ranges;
};

// Загружает страницу. Навигационные страницы берутся из кэша (кроме
// --refresh), страницы с IP-диапазонами запрашиваются заново и не кэшируются.
LoadedPage load_page(const std::string &url, ipsuip::NavCache &cache,
                     bool refresh, const ipsuip::ProgressCallback &progress) {
  if (!refresh)
    if (const std::vector<Target> *cached = cache.find(url))
      return LoadedPage{*cached, {}};

  PageContent page = fetch_page(url, progress);
  // Страница с IP-диапазонами: всегда заново и никогда в кэш.
  if (!page.ip_ranges.empty()) return LoadedPage{{}, std::move(page.ip_ranges)};
  if (page.targets.empty()) return LoadedPage{{}, {}};

  cache.store(url, page.targets);
  if (!cache.save())
    std::cerr << "Предупреждение: не удалось записать кэш " << cache.path()
              << "\n";
  return LoadedPage{std::move(page.targets), {}};
}

bool write_lines(const std::filesystem::path &path,
                 const std::vector<std::string> &lines) {
  std::ofstream ofs(path);
  if (!ofs) return false;
  for (const auto &line : lines) ofs << line << '\n';
  ofs.flush();
  return static_cast<bool>(ofs);
}

// Скачивает все IP цели и пишет два файла: диапазоны и CIDR.
// known — уже загруженные диапазоны (страница города), чтобы не качать дважды.
int save_all_ips(const Target &target, const std::vector<std::string> &known,
                 const std::string &output_dir,
                 const ipsuip::ProgressCallback &progress) {
  std::vector<std::string> ranges = known;
  if (ranges.empty()) ranges = fetch_ip_ranges(all_ip_url(target), progress);

  if (ranges.empty()) {
    std::cerr << "Диапазоны IP не найдены: " << target_label(target) << "\n";
    return 1;
  }

  std::error_code ec;
  const std::filesystem::path dir(output_dir);
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    std::cerr << "Не удалось создать каталог " << dir << ": " << ec.message()
              << "\n";
    return 1;
  }

  const std::string base = ipsuip::sanitize_filename(target_label(target));
  const std::filesystem::path ranges_path = dir / (base + ".ranges.txt");
  const std::filesystem::path cidr_path = dir / (base + ".cidr.txt");
  const std::vector<std::string> cidrs = ipsuip::ranges_to_cidr(ranges);

  if (!write_lines(ranges_path, ranges) || !write_lines(cidr_path, cidrs)) {
    std::cerr << "Ошибка записи файлов в " << dir << "\n";
    return 1;
  }

  std::cout << "Диапазоны IP (" << ranges.size()
            << "): " << ranges_path.string() << "\n";
  std::cout << "CIDR (" << cidrs.size() << "): " << cidr_path.string() << "\n";
  return 0;
}

struct LevelArg {
  bool given = false;
  std::string value; // пусто — вывести список этого уровня
};

struct Options {
  bool tui = false;
  bool help = false;
  bool version = false;
  bool refresh = false;
  bool quiet = false;      // --quiet — не показывать прогресс в stderr
  std::string output_dir;
  std::string url;
  std::string proxy;       // --proxy URL (пустая — не использовать)
  bool no_proxy = false;   // --no-proxy — явно отключить прокси
  LevelArg continent, country, province, city;
};

void print_usage(std::ostream &os) {
  os << "Списки IP-адресов suip.biz\n"
        "\n"
        "Использование:\n"
        "  ipsuip --tui\n"
        "      интерактивный браузер иерархии\n"
        "  ipsuip --mainland\n"
        "      список континентов\n"
        "  ipsuip --mainland EU --country\n"
        "      список стран континента\n"
        "  ipsuip --mainland EU --country RU --province\n"
        "      список регионов страны\n"
        "  ipsuip --mainland EU --country RU --province Chelyabinsk --city\n"
        "      список городов региона\n"
        "  ipsuip --mainland EU --country RU --province Chelyabinsk --city Kartaly\n"
        "      диапазоны IP цели\n"
        "  ipsuip ... --output DIR\n"
        "      сохранить диапазоны (RANGE и CIDR) в каталог DIR\n"
        "\n"
        "Флаг уровня без значения выводит список этого уровня, со значением —\n"
        "выбирает цель по коду или названию. --refresh игнорирует кэш\n"
        "навигационных страниц. --help показывает эту справку.\n"
        "--version печатает версию и хэш коммита.\n"
        "--quiet отключает индикатор прогресса загрузки в stderr.\n"
        "\n"
        "Прокси: --proxy URL устанавливает прокси для всех запросов\n"
        "  (http://[user:pass@]host:port, socks5://host:port).\n"
        "  --no-proxy отключает прокси из конфига.\n"
        "  Без флагов используется proxy из config.json.\n"
        "\n"
        "Кэш: ~/.cache/ipsuip/data.json. Страницы с IP-адресами не кэшируются.\n";
}

int run_console(const Options &opt,
                const ipsuip::ProgressCallback &progress) {
  ipsuip::NavCache cache;
  LoadedPage page = load_page(kRootUrl, cache, opt.refresh, progress);

  const struct {
    TargetLevel level;
    const LevelArg *arg;
  } steps[] = {
      {TargetLevel::Continent, &opt.continent},
      {TargetLevel::Country, &opt.country},
      {TargetLevel::Province, &opt.province},
      {TargetLevel::City, &opt.city},
  };

  Target selected;
  bool have_selected = false;

  for (const auto &step : steps) {
    if (!step.arg->given) continue;

    if (!page.ranges.empty()) {
      std::cerr << "У цели " << target_label(selected)
                << " нет вложенных уровней\n";
      return 1;
    }

    if (step.arg->value.empty()) {
      const size_t count = print_targets(page.targets, &step.level);
      if (count == 0) {
        std::cerr << "Нет элементов уровня " << level_name(step.level) << "\n";
        return 1;
      }
      return 0;
    }

    const Target *found = find_target(page.targets, step.level, step.arg->value);
    if (!found) {
      std::cerr << "Не найдено: " << level_name(step.level) << " \""
                << step.arg->value << "\""
                << (have_selected ? "" : " (укажите родительские уровни)")
                << "\n";
      return 1;
    }

    selected = *found;
    have_selected = true;
    page = load_page(selected.address, cache, opt.refresh, progress);
  }

  if (!opt.output_dir.empty()) {
    if (!have_selected) {
      std::cerr << "--output требует выбранной цели\n";
      return 1;
    }
    // Для города диапазоны уже на его странице, для остальных уровней —
    // отдельная выгрузка всех IP.
    return save_all_ips(selected, page.ranges, opt.output_dir, progress);
  }

  if (!page.ranges.empty()) {
    for (const auto &range : page.ranges) std::cout << range << "\n";
    return 0;
  }

  if (print_targets(page.targets, nullptr) == 0) {
    std::cerr << "На странице нет элементов\n";
    return 1;
  }
  return 0;
}

bool parse_args(int argc, char **argv, Options &opt, std::string &error) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];

    const auto take_value = [&](LevelArg &dst) {
      dst.given = true;
      if (i + 1 < argc && argv[i + 1][0] != '-') dst.value = argv[++i];
    };

    if (arg == "--help" || arg == "-h")
      opt.help = true;
    else if (arg == "--version")
      opt.version = true;
    else if (arg == "--tui")
      opt.tui = true;
    else if (arg == "--refresh")
      opt.refresh = true;
    else if (arg == "--quiet")
      opt.quiet = true;
    else if (arg == "--no-proxy")
      opt.no_proxy = true;
    else if (arg == "--proxy") {
      if (i + 1 >= argc) {
        error = "--proxy требует URL";
        return false;
      }
      opt.proxy = argv[++i];
    }
    else if (arg == "--mainland" || arg == "--continent")
      take_value(opt.continent);
    else if (arg == "--country")
      take_value(opt.country);
    else if (arg == "--province")
      take_value(opt.province);
    else if (arg == "--city")
      take_value(opt.city);
    else if (arg == "--output") {
      if (i + 1 >= argc) {
        error = "--output требует каталог";
        return false;
      }
      opt.output_dir = argv[++i];
    } else if (arg.rfind("--", 0) == 0) {
      error = "Неизвестный флаг: " + arg;
      return false;
    } else {
      opt.url = arg;
    }
  }
  return true;
}

} // namespace

int main(int argc, char **argv) {
  Options opt;
  std::string error;
  if (!parse_args(argc, argv, opt, error)) {
    std::cerr << error << "\n\n";
    print_usage(std::cerr);
    return 1;
  }

  if (opt.help) {
    print_usage(std::cout);
    return 0;
  }

  if (opt.version) {
    std::cout << "ipsuip " << IPSUIP_VERSION_STRING
              << " (commit " << IPSUIP_GIT_COMMIT << ")\n";
    return 0;
  }

  // В TUI прогресс рисуется в статусной строке, а не в stderr.
  ipsuip::ProgressDisplay progress(!opt.quiet && !opt.tui);
  const ipsuip::ProgressCallback on_progress = progress.callback();

  try {
    // Применяем прокси: CLI > конфиг > ничего
    if (opt.no_proxy) {
      ipsuip::clear_proxy();
    } else if (!opt.proxy.empty()) {
      ipsuip::set_proxy(opt.proxy);
    } else {
      const ipsuip::Config cfg = ipsuip::load_config();
      if (!cfg.proxy.empty()) {
        std::string proxy_error;
        if (!ipsuip::is_valid_proxy(cfg.proxy, &proxy_error))
          std::cerr << "Предупреждение: некорректный прокси в конфиге: "
                    << proxy_error << "\n";
        else
          ipsuip::set_proxy(cfg.proxy);
      }
    }

    if (opt.tui)
      // Интерактивный режим: по умолчанию начинаем с корня иерархии.
      return ipsuip::run_tui(opt.url.empty() ? kRootUrl : opt.url);

    const bool console = opt.continent.given || opt.country.given ||
                         opt.province.given || opt.city.given;

    if (opt.url.empty() && !console) {
      print_usage(std::cerr);
      return 1;
    }

    if (!opt.url.empty()) {
      // Прямой URL страницы: печать её содержимого.
      const PageContent page = fetch_page(opt.url, on_progress);
      progress.finish();
      if (!page.ip_ranges.empty())
        for (const auto &range : page.ip_ranges) std::cout << range << "\n";
      else
        print_targets(page.targets, nullptr);
      return 0;
    }

    const int result = run_console(opt, on_progress);
    progress.finish();
    return result;

  } catch (std::exception &e) {
    progress.finish();
    std::cerr << e.what() << std::endl;
    return 1;
  }
}
