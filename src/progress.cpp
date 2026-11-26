// SPDX-License-Identifier: MIT
#include "progress.h"

#include <algorithm>
#include <cstdio>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace ipsuip {
namespace {

constexpr auto kUpdateInterval = std::chrono::milliseconds(100);

std::string scaled(double value, const char *unit) {
  char buf[32];
  if (value >= 100)
    std::snprintf(buf, sizeof(buf), "%.0f %s", value, unit);
  else if (value >= 10)
    std::snprintf(buf, sizeof(buf), "%.1f %s", value, unit);
  else
    std::snprintf(buf, sizeof(buf), "%.2f %s", value, unit);
  return buf;
}

} // namespace

std::string human_bytes(std::uint64_t bytes) {
  double value = static_cast<double>(bytes);
  if (value >= 1'000'000'000) return scaled(value / 1'000'000'000, "GB");
  if (value >= 1'000'000) return scaled(value / 1'000'000, "MB");
  if (value >= 1'000) return scaled(value / 1'000, "kB");
  return scaled(value, "B");
}

std::string format_speed(double bytes_per_second) {
  double value = bytes_per_second;
  if (value >= 1'000'000'000) return scaled(value / 1'000'000'000, "GB/s");
  if (value >= 1'000'000) return scaled(value / 1'000'000, "MB/s");
  if (value >= 1'000) return scaled(value / 1'000, "kB/s");
  return scaled(value, "B/s");
}

bool ProgressDisplay::is_terminal() {
#if defined(_WIN32)
  return _isatty(_fileno(stderr)) != 0;
#else
  return isatty(fileno(stderr)) != 0;
#endif
}

ProgressDisplay::ProgressDisplay(bool enabled)
    : enabled_(enabled && is_terminal()),
      start_(std::chrono::steady_clock::now()),
      last_update_(start_) {}

ProgressDisplay::~ProgressDisplay() { finish(); }

std::function<bool(std::uint64_t, std::uint64_t)> ProgressDisplay::callback() {
  return [this](std::uint64_t downloaded, std::uint64_t total) -> bool {
    render(downloaded, total);
    return true; // продолжать загрузку
  };
}

void ProgressDisplay::render(std::uint64_t downloaded, std::uint64_t total) {
  if (!enabled_) return;

  const auto now = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(mutex_);

  // Первое обновление рисуем сразу, последующие — не чаще 10 раз в секунду.
  if (active_ && now - last_update_ < kUpdateInterval) return;
  last_update_ = now;

  const double elapsed =
      std::chrono::duration<double>(now - start_).count();
  const double speed = elapsed > 0 ? static_cast<double>(downloaded) / elapsed : 0;

  std::string line = "\r  Загрузка: " + human_bytes(downloaded);
  if (total > 0) {
    const int pct = static_cast<int>(100.0 * downloaded / total);
    line += " / " + human_bytes(total) + " (" +
            std::to_string(std::clamp(pct, 0, 100)) + "%)";
  }
  line += "  " + format_speed(speed);

  // Стираем предыдущий (более длинный) вывод.
  std::fputs("\r", stderr);
  if (last_width_ > line.size())
    std::fputs(std::string(last_width_ - line.size(), ' ').c_str(), stderr);
  std::fputs(line.c_str(), stderr);
  std::fflush(stderr);

  last_width_ = line.size();
  active_ = true;
}

void ProgressDisplay::finish() {
  if (!enabled_) return;
  std::lock_guard<std::mutex> lock(mutex_);
  if (!active_) return;

  std::fputs("\r", stderr);
  if (last_width_)
    std::fputs(std::string(last_width_, ' ').c_str(), stderr);
  std::fputs("\r", stderr);
  std::fflush(stderr);

  active_ = false;
  last_width_ = 0;
  start_ = std::chrono::steady_clock::now();
  last_update_ = start_;
}

} // namespace ipsuip
