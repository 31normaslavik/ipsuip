#pragma once

/** Интерактивный TUI-браузер иерархии suip.biz (ncursesw). **/

#include <string>

namespace ipsuip {

// Запускает интерактивный экран от указанной страницы-корня
// (обычно https://suip.biz/?act=all-country-ip).
// Возвращает код возврата процесса.
int run_tui(const std::string &root_url);

} // namespace ipsuip
