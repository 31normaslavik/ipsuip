#pragma once

/** Консольный индикатор прогресса загрузки (обновление в stderr). **/

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace ipsuip {

// Индикатор прогресса для CLI. Выводит байты и проценты (если известен
// Content-Length), иначе объём и скорость. Пишет только в интерактивный
// терминал (stderr), обновления троттлятся. Потокобезопасен.
class ProgressDisplay {
public:
  explicit ProgressDisplay(bool enabled = true);
  ~ProgressDisplay();

  ProgressDisplay(const ProgressDisplay &) = delete;
  ProgressDisplay &operator=(const ProgressDisplay &) = delete;

  // Возвращает коллбэк для GetOptions::progress. downloaded — байт тела,
  // total — Content-Length либо 0, если размер неизвестен.
  std::function<bool(std::uint64_t, std::uint64_t)> callback();

  // Очищает строку прогресса (если она отображалась).
  void finish();

  // true, когда stderr подключён к терминалу.
  static bool is_terminal();

private:
  void render(std::uint64_t downloaded, std::uint64_t total);

  bool enabled_;
  mutable std::mutex mutex_;
  std::chrono::steady_clock::time_point start_;
  std::chrono::steady_clock::time_point last_update_;
  std::size_t last_width_ = 0;
  bool active_ = false;
};

// "1.23 MB", "456 kB", "789 B" — десятические приставки.
std::string human_bytes(std::uint64_t bytes);

// "1.23 MB/s", "456 kB/s", ...
std::string format_speed(double bytes_per_second);

} // namespace ipsuip
