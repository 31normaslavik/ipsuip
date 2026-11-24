#include "ipsuip_tui.h"

#include "ip_convert.h"
#include "suip_cache.h"
#include "suip_model.h"

#include <ncurses.h>

#include <algorithm>
#include <clocale>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
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
  std::string status;       // сообщение в статусной строке
  bool status_error = false;
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
        "↑/↓ k/j выбор  PgUp/PgDn Home/End  Enter открыть  "
        "Backspace/Esc назад  q выход";
    attron(A_DIM);
    mvaddstr(hint_row, 0, clip_utf8(hint, cols).c_str());
    attroff(A_DIM);
  }

  refresh();
}

// ---- загрузка страниц --------------------------------------------------

// Загружает страницу: навигационные берутся из общего кэша, страницы
// с IP-адресами всегда запрашиваются заново и в кэш не попадают.
// Библиотека http_client пишет служебные сообщения в stdout/stderr,
// поэтому на время запроса оба потока перенаправляются в буфер.
PageContent load(State &st, const std::string &url) {
  if (const std::vector<Target> *cached = st.cache.find(url))
    return PageContent{*cached, {}};

  set_status(st, "Загрузка " + url + " ...", false);
  draw(st);

  std::ostringstream sink;
  std::streambuf *old_out = std::cout.rdbuf(sink.rdbuf());
  std::streambuf *old_err = std::cerr.rdbuf(sink.rdbuf());

  PageContent page;
  try {
    page = fetch_page(url);
  } catch (...) {
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    throw;
  }
  std::cout.rdbuf(old_out);
  std::cerr.rdbuf(old_err);

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

bool write_lines(const std::string &path, const std::vector<std::string> &lines) {
  std::ofstream ofs(path);
  if (!ofs) return false;
  for (const auto &line : lines) ofs << line << '\n';
  ofs.flush();
  return static_cast<bool>(ofs);
}

std::vector<std::string> to_cidr(const std::vector<std::string> &ranges) {
  return ranges_to_cidr(ranges);
}

// Меню из двух пунктов. Возвращает false при отмене (Esc).
bool choose_format(const std::string &name, size_t count, SaveFormat &format) {
  static const char *kItems[] = {"Сохранить как Range", "Сохранить как Cidr"};
  int sel = 0;

  for (;;) {
    int rows = 0;
    int cols = 0;
    getmaxyx(stdscr, rows, cols);

    int w = std::min(cols - 2, 52);
    if (w < 24) w = cols > 4 ? cols - 2 : 1;
    const int h = 8;
    const int y = std::max(0, (rows - h) / 2);
    const int x = std::max(0, (cols - w) / 2);

    WINDOW *win = newwin(h, w, y, x);
    if (!win) return false;
    keypad(win, TRUE);
    box(win, 0, 0);

    mvwaddstr(win, 0, 2, clip_utf8(" " + name + " ", w - 4).c_str());
    mvwaddstr(win, 1, 2,
              clip_utf8(std::to_string(count) + " диапазон(ов)", w - 4).c_str());

    for (int i = 0; i < 2; ++i) {
      const std::string line =
          std::string(i == sel ? "> " : "  ") + kItems[i];
      if (i == sel) wattron(win, A_REVERSE);
      mvwaddstr(win, 3 + i, 2, clip_utf8(line, w - 4).c_str());
      if (i == sel) wattroff(win, A_REVERSE);
    }

    mvwaddstr(win, h - 2, 2,
              clip_utf8("Enter - выбрать, Esc - отмена", w - 4).c_str());
    wrefresh(win);

    const int ch = wgetch(win);
    delwin(win);
    touchwin(stdscr);

    if (ch == KEY_UP || ch == 'k' || ch == KEY_DOWN || ch == 'j') {
      sel = (sel + 1) % 2; // пунктов всего два
    } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
      format = sel == 0 ? SaveFormat::Ranges : SaveFormat::Cidr;
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
  if (!choose_format(name, ranges.size(), format)) {
    set_status(st, "Сохранение отменено", false);
    return;
  }

  const std::vector<std::string> lines =
      format == SaveFormat::Cidr ? to_cidr(ranges) : ranges;
  const std::string path = sanitize_filename(name) +
                           (format == SaveFormat::Cidr ? ".cidr.txt"
                                                       : ".ranges.txt");

  if (!write_lines(path, lines)) {
    set_status(st, "Ошибка записи файла: " + path, true);
    return;
  }

  set_status(st, "Сохранено " + std::to_string(lines.size()) + " строк в " +
                     path,
             false);
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

  std::vector<std::string> ranges;
  std::ostringstream sink;
  std::streambuf *old_out = std::cout.rdbuf(sink.rdbuf());
  std::streambuf *old_err = std::cerr.rdbuf(sink.rdbuf());

  try {
    ranges = fetch_ip_ranges(f.download_url);
  } catch (...) {
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    throw;
  }
  std::cout.rdbuf(old_out);
  std::cerr.rdbuf(old_err);

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
  set_escdelay(25);
  if (has_colors()) {
    start_color();
    use_default_colors();
    init_pair(1, COLOR_RED, -1);
  }

  State st;
  st.root_url = root_url;

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
