#include "ipsuip_tui.h"

#include "ip_convert.h"
#include "config.h"
#include "progress.h"
#include "proxy.h"
#include "suip_cache.h"
#include "suip_model.h"

#include <curses.h>

#include <algorithm>
#include <chrono>
#include <clocale>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ipsuip {
namespace {

// ---- состояние ---------------------------------------------------------

// Строка списка кадра: либо переход к дочерней цели, либо выгрузка всех IP
// текущей страницы (download_all).
struct Row {
  Target target;
  bool download_all = false;
};

struct Frame {
  std::string url;           // адрес страницы, отображаемой в этом кадре
  std::string title;         // подпись в breadcrumb
  TargetLevel level = TargetLevel::Continent; // уровень самой страницы
  std::string download_url;  // URL выгрузки всех IP страницы
  std::vector<Row> items;    // строки списка, отсортированные по названию
  int selected = 0;
  int top = 0;
};

struct State {
  std::string root_url;
  std::vector<Frame> stack; // стек навигации
  NavCache cache;           // общий JSON-кэш навигационных страниц
  Config config;            // конфиг (прокси), сохраняется из меню настроек
  std::string status;       // сообщение в статусной строке
  bool status_error = false;
};

// Состояние одного запроса для отрисовки прогресса в статусной строке.
struct DownloadMeter {
  std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point last_draw{};
  std::string last_text;
};

enum class SaveFormat { Ranges, Cidr };

// ---- вспомогательные ---------------------------------------------------

// Обрезает UTF-8 строку до max_cells знакомест (каждый кодпоинт = 1 знакоместо).
std::string clip_utf8(const std::string &s, int max_cells) {
  if (max_cells <= 0) return {};
  std::string out;
  out.reserve(s.size());
  int cells = 0;

  for (size_t i = 0; i < s.size();) {
    const unsigned char c = static_cast<unsigned char>(s[i]);

    if (c < 0x20 || c == 0x7F) { // управляющие символы -> пробел
      if (cells + 1 > max_cells) break;
      out += ' ';
      ++i;
      ++cells;
      continue;
    }

    size_t len = 1;
    if ((c & 0xE0) == 0xC0) len = 2;
    else if ((c & 0xF0) == 0xE0) len = 3;
    else if ((c & 0xF8) == 0xF0) len = 4;
    if (i + len > s.size()) break;

    if (cells + 1 > max_cells) break;
    out.append(s, i, len);
    i += len;
    ++cells;
  }
  return out;
}

int list_height() {
  int rows = 0;
  int cols = 0;
  getmaxyx(stdscr, rows, cols);
  (void)cols;
  const int h = rows - 4; // 2 строки сверху, статус и подсказка снизу
  return h > 0 ? h : 1;
}

void set_status(State &st, std::string text, bool error) {
  st.status = std::move(text);
  st.status_error = error;
}

void clear_status(State &st) {
  st.status.clear();
  st.status_error = false;
}

// Коллбэк загрузки: проценты/байты/скорость в статусной строке.
// Обновление экрана троттлится; draw() вызывается напрямую, так как запрос
// синхронный и цикл обработки клавиш в это время не работает.
void draw(State &st);

ProgressCallback make_progress(State &st) {
  auto meter = std::make_shared<DownloadMeter>();
  return [&st, meter](std::uint64_t downloaded, std::uint64_t total) -> bool {
    const auto now = std::chrono::steady_clock::now();
    const double elapsed =
        std::chrono::duration<double>(now - meter->start).count();
    const double speed =
        elapsed > 0 ? static_cast<double>(downloaded) / elapsed : 0;

    std::string text = "Загрузка: " + human_bytes(downloaded);
    if (total > 0) {
      const int pct = static_cast<int>(100.0 * downloaded / total);
      text += " / " + human_bytes(total) + " (" + std::to_string(pct) + "%)";
    }
    text += "  " + format_speed(speed);

    if (text != meter->last_text &&
        now - meter->last_draw >= std::chrono::milliseconds(100)) {
      meter->last_text = text;
      meter->last_draw = now;
      set_status(st, text, false);
      draw(st);
    }
    return true;
  };
}

// ---- отрисовка ---------------------------------------------------------

void draw(State &st) {
  int rows = 0;
  int cols = 0;
  getmaxyx(stdscr, rows, cols);
  erase();

  Frame *f = st.stack.empty() ? nullptr : &st.stack.back();
  const int list_top = 2;
  const int status_row = rows - 2;
  const int hint_row = rows - 1;

  if (f) {
    // breadcrumb: путь от корня до текущей страницы
    std::string crumb;
    for (size_t i = 0; i < st.stack.size(); ++i) {
      if (i) crumb += " > ";
      crumb += st.stack[i].title;
    }
    attron(A_BOLD);
    mvaddstr(0, 0, clip_utf8(crumb, cols).c_str());
    attroff(A_BOLD);
    if (rows > 1) mvhline(1, 0, ACS_HLINE, cols);

    const int count = static_cast<int>(f->items.size());
    const int height = list_height();

    if (f->selected < 0) f->selected = 0;
    if (f->selected >= count) f->selected = count > 0 ? count - 1 : 0;
    if (f->selected < f->top) f->top = f->selected;
    if (f->selected >= f->top + height) f->top = f->selected - height + 1;
    if (f->top < 0) f->top = 0;

    const int num_width =
        static_cast<int>(std::to_string(std::max(count, 1)).size());

    for (int i = 0; i < height; ++i) {
      const int idx = f->top + i;
      if (idx >= count) break;

      const Row &row = f->items[idx];
      const std::string label =
          row.download_all
              ? "Скачать все адреса " + f->title
              : (row.target.location.empty() ? row.target.code
                                             : row.target.location);

      std::string num = std::to_string(idx + 1);
      if (static_cast<int>(num.size()) < num_width)
        num.insert(0, num_width - num.size(), ' ');

      const std::string line = clip_utf8(" " + num + ". " + label, cols);
      const int y = list_top + i;

      if (idx == f->selected) attron(A_REVERSE);
      mvaddstr(y, 0, line.c_str());
      if (idx == f->selected) attroff(A_REVERSE);
    }
  }

  // статусная строка
  if (status_row >= 0) {
    std::string line;
    bool error = false;

    if (!st.status.empty()) {
      line = st.status;
      error = st.status_error;
    } else if (f && !f->items.empty()) {
      const Row &row = f->items[f->selected];
      if (row.download_all) {
        line = "Скачать все IP: " + f->title;
      } else {
        const Target &t = row.target;
        line = std::string("[") + level_name(t.level) + "] " +
               (t.location.empty() ? t.code : t.location) + "  -  " +
               std::to_string(f->selected + 1) + "/" +
               std::to_string(f->items.size());
      }
    } else if (f) {
      line = "Элементов нет";
    }

    if (!line.empty()) {
      if (error && has_colors()) attron(COLOR_PAIR(1));
      mvaddstr(status_row, 0, clip_utf8(line, cols).c_str());
      if (error && has_colors()) attroff(COLOR_PAIR(1));
    }
  }

  // подсказка по клавишам
  if (hint_row >= 0) {
    const std::string hint =
        "↑/↓ k/j выбор  Enter открыть  s настройки  "
        "PgUp/PgDn Home/End  Backspace/Esc назад  q выход";
    attron(A_DIM);
    mvaddstr(hint_row, 0, clip_utf8(hint, cols).c_str());
    attroff(A_DIM);
  }

  refresh();
}

// ---- загрузка страниц --------------------------------------------------

// Загружает страницу: навигационные берутся из общего кэша, страницы
// с IP-адресами всегда запрашиваются заново и в кэш не попадают.
// Прогресс загрузки рисуется в статусной строке через коллбэк.
PageContent load(State &st, const std::string &url) {
  if (const std::vector<Target> *cached = st.cache.find(url))
    return PageContent{*cached, {}};

  set_status(st, "Загрузка " + url + " ...", false);
  draw(st);

  PageContent page = fetch_page(url, make_progress(st));

  // Страницы с IP-адресами (у них может быть только ссылка на себя)
  // в кэш не попадают.
  if (page.ip_ranges.empty() && !page.targets.empty()) {
    st.cache.store(url, page.targets);
    st.cache.save();
  }

  clear_status(st);
  return page;
}

Frame make_frame(const std::string &url, const std::string &title,
                 TargetLevel level, const PageContent &page) {
  Frame f;
  f.url = url;
  f.title = title;
  f.level = level;

  // Собственная ссылка страницы (её же &all-download) в списке не нужна,
  // но из неё берётся URL выгрузки всех IP; если её нет — конструируем сами.
  const Target *self = nullptr;
  for (const auto &t : page.targets)
    if (t.address == url) {
      self = &t;
      break;
    }

  for (const auto &t : page.targets)
    if (&t != self) f.items.push_back(Row{t, false});

  std::stable_sort(f.items.begin(), f.items.end(),
                   [](const Row &a, const Row &b) {
                     const std::string &ka = a.target.location.empty()
                                                 ? a.target.code
                                                 : a.target.location;
                     const std::string &kb = b.target.location.empty()
                                                 ? b.target.code
                                                 : b.target.location;
                     return ka < kb;
                   });

  // У города парной ссылки "&all" нет — его страница и есть список адресов.
  if (self && level != TargetLevel::City) {
    f.download_url = all_ip_url(*self);
    f.items.insert(f.items.begin(), Row{*self, true});
  }
  return f;
}

// ---- сохранение IP-списков ---------------------------------------------

bool write_lines(const std::filesystem::path &path,
                 const std::vector<std::string> &lines) {
  std::ofstream ofs(path);
  if (!ofs) return false;
  for (const auto &line : lines) ofs << line << '\n';
  ofs.flush();
  return static_cast<bool>(ofs);
}

std::vector<std::string> to_cidr(const std::vector<std::string> &ranges) {
  return ranges_to_cidr(ranges);
}

// Универсальное меню в центре экрана. Возвращает номер пункта или -1 (Esc).
int choose_item(const std::string &title, const std::vector<std::string> &items,
                const std::string &extra = {}) {
  int sel = 0;

  for (;;) {
    int rows = 0;
    int cols = 0;
    getmaxyx(stdscr, rows, cols);

    int w = std::min(cols - 2, 56);
    if (w < 16) w = cols > 4 ? cols - 2 : 1;
    const int h = std::min(rows > 0 ? rows : 1,
                           5 + static_cast<int>(items.size()) +
                               (extra.empty() ? 0 : 1));
    const int y = std::max(0, (rows - h) / 2);
    const int x = std::max(0, (cols - w) / 2);

    WINDOW *win = newwin(h, w, y, x);
    if (!win) return -1;
    keypad(win, TRUE);
    box(win, 0, 0);

    int line = 1;
    mvwaddstr(win, 0, 2, clip_utf8(" " + title + " ", w - 4).c_str());
    if (!extra.empty()) {
      mvwaddstr(win, line++, 2, clip_utf8(extra, w - 4).c_str());
    }
    line++; // пустая строка перед пунктами

    for (size_t i = 0; i < items.size() && line < h - 2; ++i, ++line) {
      const std::string text =
          std::string(i == static_cast<size_t>(sel) ? "> " : "  ") + items[i];
      if (i == static_cast<size_t>(sel)) wattron(win, A_REVERSE);
      mvwaddstr(win, line, 2, clip_utf8(text, w - 4).c_str());
      if (i == static_cast<size_t>(sel)) wattroff(win, A_REVERSE);
    }

    if (h >= 2)
      mvwaddstr(win, h - 2, 2,
                clip_utf8("↑/↓ выбор  Enter - выбрать  Esc - отмена", w - 4)
                    .c_str());
    wrefresh(win);

    const int ch = wgetch(win);
    delwin(win);
    touchwin(stdscr);

    if (ch == KEY_UP || ch == 'k') {
      if (sel > 0) --sel;
    } else if (ch == KEY_DOWN || ch == 'j') {
      if (sel + 1 < static_cast<int>(items.size())) ++sel;
    } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
      return sel;
    } else if (ch == 27) {
      return -1;
    }
  }
}

std::filesystem::path build_save_dir(const State &st,
                                     const std::string &leaf_name) {
  std::filesystem::path dir;
  // Первый кадр - корень ("suip.biz"), его в путь не включаем.
  for (size_t i = 1; i < st.stack.size(); ++i)
    dir /= sanitize_path_segment(st.stack[i].title);
  // Если сохраняемая цель уже представлена текущим кадром (download_all),
  // не дублируем конечный сегмент.
  if (st.stack.empty() || st.stack.back().title != leaf_name)
    dir /= sanitize_path_segment(leaf_name);
  if (dir.empty()) dir = sanitize_path_segment(leaf_name);
  return dir;
}

// Диалог сохранения: формат + чекбокс создания поддиректорий.
// Состояние чекбокса берётся из конфига и сохраняется при изменении.
bool choose_save_options(State &st, const std::string &name, size_t count,
                         SaveFormat &format) {
  int sel = 0;
  bool create_subdirs = st.config.create_subdirs;

  for (;;) {
    int rows = 0;
    int cols = 0;
    getmaxyx(stdscr, rows, cols);

    int w = std::min(cols - 2, 64);
    if (w < 20) w = cols > 4 ? cols - 2 : 1;
    const int h = std::min(rows > 0 ? rows : 1, 10);
    const int y = std::max(0, (rows - h) / 2);
    const int x = std::max(0, (cols - w) / 2);

    WINDOW *win = newwin(h, w, y, x);
    if (!win) return false;
    keypad(win, TRUE);
    box(win, 0, 0);

    mvwaddstr(win, 0, 2, clip_utf8(" " + name + " ", w - 4).c_str());
    mvwaddstr(win, 1, 2,
              clip_utf8(std::to_string(count) + " диапазон(ов)", w - 4).c_str());

    const std::vector<std::string> items = {
        "Сохранить как Range",
        "Сохранить как Cidr",
        std::string("[") + (create_subdirs ? "x" : " ") +
            "] Создавать поддиректории",
    };

    for (size_t i = 0; i < items.size(); ++i) {
      const int line = 3 + static_cast<int>(i);
      if (line >= h - 2) break;
      const std::string text =
          std::string(i == static_cast<size_t>(sel) ? "> " : "  ") + items[i];
      if (i == static_cast<size_t>(sel)) wattron(win, A_REVERSE);
      mvwaddstr(win, line, 2, clip_utf8(text, w - 4).c_str());
      if (i == static_cast<size_t>(sel)) wattroff(win, A_REVERSE);
    }

    if (h >= 2)
      mvwaddstr(win, h - 2, 2,
                clip_utf8("↑/↓ выбор  Enter/Space - выбрать  Esc - отмена",
                          w - 4)
                    .c_str());
    wrefresh(win);

    const int ch = wgetch(win);
    delwin(win);
    touchwin(stdscr);

    if (ch == KEY_UP || ch == 'k') {
      if (sel > 0) --sel;
    } else if (ch == KEY_DOWN || ch == 'j') {
      if (sel + 1 < static_cast<int>(items.size())) ++sel;
    } else if (ch == ' ' && sel == 2) {
      create_subdirs = !create_subdirs;
    } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
      if (sel == 2) {
        create_subdirs = !create_subdirs;
        continue;
      }
      format = sel == 0 ? SaveFormat::Ranges : SaveFormat::Cidr;
      if (st.config.create_subdirs != create_subdirs) {
        st.config.create_subdirs = create_subdirs;
        if (!save_config(st.config))
          set_status(st, "Не удалось сохранить config.json", true);
      }
      return true;
    } else if (ch == 27) {
      return false;
    }
  }
}

void offer_save(State &st, const std::string &name,
                const std::vector<std::string> &ranges) {
  if (ranges.empty()) {
    set_status(st, "Диапазоны IP не найдены", true);
    return;
  }

  SaveFormat format = SaveFormat::Ranges;
  if (!choose_save_options(st, name, ranges.size(), format)) {
    set_status(st, "Сохранение отменено", false);
    return;
  }

  const std::vector<std::string> lines =
      format == SaveFormat::Cidr ? to_cidr(ranges) : ranges;
  const std::string file_name = sanitize_filename(name) +
                                (format == SaveFormat::Cidr ? ".cidr.txt"
                                                            : ".ranges.txt");

  std::filesystem::path out_path;
  if (st.config.create_subdirs) {
    std::filesystem::path dir = build_save_dir(st, name);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
      set_status(st, "Ошибка создания каталога: " + dir.string(), true);
      return;
    }
    out_path = dir / file_name;
  } else {
    out_path = file_name;
  }

  if (!write_lines(out_path, lines)) {
    set_status(st, "Ошибка записи файла: " + out_path.string(), true);
    return;
  }

  set_status(st, "Сохранено " + std::to_string(lines.size()) + " строк в " +
                     out_path.string(),
             false);
}

// ---- настройки ---------------------------------------------------------

// Диалог ввода прокси. Пустая строка отключает прокси.
// Валидация и применение выполняются сразу по Enter, конфиг сохраняется.
void proxy_dialog(State &st) {
  std::string input = st.config.proxy;
  std::string error;

  for (;;) {
    int rows = 0;
    int cols = 0;
    getmaxyx(stdscr, rows, cols);

    const int w = std::max(20, std::min(cols - 2, 72));
    const int h = std::min(rows > 0 ? rows : 1, error.empty() ? 8 : 9);
    const int y = std::max(0, (rows - h) / 2);
    const int x = std::max(0, (cols - w) / 2);

    WINDOW *win = newwin(h, w, y, x);
    if (!win) return;
    keypad(win, TRUE);
    box(win, 0, 0);

    mvwaddstr(win, 0, 2, " Прокси ");
    mvwaddstr(win, 2, 2, clip_utf8(
        "Формат: http://[user:pass@]host:port, socks5://host:port", w - 4).c_str());
    mvwaddstr(win, 3, 2, clip_utf8("Значение: " + input + "_", w - 4).c_str());
    if (!error.empty()) {
      if (has_colors()) wattron(win, COLOR_PAIR(1));
      mvwaddstr(win, 4, 2, clip_utf8(error, w - 4).c_str());
      if (has_colors()) wattroff(win, COLOR_PAIR(1));
    }
    mvwaddstr(win, h - 2, 2,
              clip_utf8("Enter - применить, Esc - отмена", w - 4).c_str());

    curs_set(1);
    wrefresh(win);
    const int ch = wgetch(win);
    curs_set(0);
    delwin(win);
    touchwin(stdscr);

    if (ch == 27) return;

    if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
      if (!is_valid_proxy(input, &error)) continue;

      if (input.empty()) {
        clear_proxy();
        set_status(st, "Прокси отключён", false);
      } else {
        set_proxy(input);
        set_status(st, "Прокси применён: " + input, false);
      }
      st.config.proxy = input;
      if (!save_config(st.config))
        set_status(st, "Прокси применён, но не сохранён в config.json", true);
      return;
    }

    if (ch == KEY_BACKSPACE || ch == 8 || ch == 127) {
      if (!input.empty()) input.pop_back();
    } else if (ch >= 32 && ch < 127) {
      input += static_cast<char>(ch);
    }
    error.clear();
  }
}

void settings_menu(State &st) {
  const int sel = choose_item(
      "Настройки", {"Очистить кэш", "Прокси", "Назад"});
  if (sel == 0) {
    st.cache.clear();
    set_status(st, "Кэш очищен", false);
  } else if (sel == 1) {
    proxy_dialog(st);
  }
}

// ---- навигация ---------------------------------------------------------

void open_target(State &st, const Target &t) {
  // Ссылка на саму себя (страница содержит свой же &all-download) — не переходим.
  if (!st.stack.empty() && t.address == st.stack.back().url) {
    set_status(st, "Это текущая страница", false);
    return;
  }

  const std::string title = t.location.empty() ? t.code : t.location;

  const PageContent page = load(st, t.address);

  // Страница с адресами (город или любой уровень с IP-списком).
  if (!page.ip_ranges.empty()) {
    offer_save(st, title, page.ip_ranges);
    return;
  }

  if (page.targets.empty()) {
    set_status(st, "На странице нет элементов: " + t.address, true);
    return;
  }

  st.stack.push_back(make_frame(t.address, title, t.level, page));
  clear_status(st);
}

// Скачивает все IP текущей страницы кадра и предлагает сохранить их.
void download_all(State &st, const Frame &f) {
  if (f.download_url.empty()) {
    set_status(st, "Нет ссылки на выгрузку всех IP", true);
    return;
  }

  set_status(st, "Загрузка всех IP: " + f.title + " ...", false);
  draw(st);

  std::vector<std::string> ranges =
      fetch_ip_ranges(f.download_url, make_progress(st));

  offer_save(st, f.title, ranges);
}

void open_row(State &st, const Row &row) {
  if (row.download_all) {
    download_all(st, st.stack.back());
    return;
  }
  open_target(st, row.target);
}

} // namespace
} // namespace ipsuip

namespace ipsuip {

int run_tui(const std::string &root_url) {
  setlocale(LC_ALL, "");

  if (!initscr()) return 1;
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
  if (has_colors()) {
    start_color();
    use_default_colors();
    init_pair(1, COLOR_RED, -1);
  }

  State st;
  st.root_url = root_url;
  st.config = load_config();
  if (!st.config.proxy.empty()) {
    std::string error;
    if (!is_valid_proxy(st.config.proxy, &error)) {
      st.config.proxy.clear();
      set_status(st, "Некорректный прокси в конфиге: " + error, true);
    } else {
      set_proxy(st.config.proxy);
    }
  }

  try {
    const PageContent &root = load(st, root_url);
    if (root.targets.empty()) {
      endwin();
      std::fprintf(stderr, "На странице нет элементов: %s\n", root_url.c_str());
      return 1;
    }
    st.stack.push_back(
        make_frame(root_url, "suip.biz", TargetLevel::Continent, root));
    clear_status(st);
  } catch (const std::exception &e) {
    endwin();
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }

  bool running = true;
  while (running) {
    draw(st);

    const int ch = getch();
    Frame &f = st.stack.back();

    switch (ch) {
      case 'q':
      case 'Q':
        running = false;
        break;

      case KEY_UP:
      case 'k':
        if (f.selected > 0) --f.selected;
        clear_status(st);
        break;

      case KEY_DOWN:
      case 'j':
        if (f.selected + 1 < static_cast<int>(f.items.size())) ++f.selected;
        clear_status(st);
        break;

      case KEY_PPAGE:
        f.selected -= list_height();
        if (f.selected < 0) f.selected = 0;
        clear_status(st);
        break;

      case KEY_NPAGE:
        f.selected += list_height();
        if (f.selected >= static_cast<int>(f.items.size()))
          f.selected = static_cast<int>(f.items.size()) - 1;
        if (f.selected < 0) f.selected = 0;
        clear_status(st);
        break;

      case KEY_HOME:
        f.selected = 0;
        clear_status(st);
        break;

      case KEY_END:
        f.selected = static_cast<int>(f.items.size()) - 1;
        if (f.selected < 0) f.selected = 0;
        clear_status(st);
        break;

      case '\n':
      case '\r':
      case KEY_ENTER: {
        if (!f.items.empty()) {
          const Row row = f.items[f.selected];
          open_row(st, row);
        }
        break;
      }

      case KEY_BACKSPACE:
      case 127:
      case 8:
      case 27:
      case KEY_LEFT:
        if (st.stack.size() > 1) {
          st.stack.pop_back();
          clear_status(st);
        }
        break;

      case 's':
      case 'S':
      case KEY_F(2):
        settings_menu(st);
        break;

      case KEY_RESIZE:
        break;

      default:
        break;
    }
  }

  endwin();
  return 0;
}

} // namespace ipsuip
