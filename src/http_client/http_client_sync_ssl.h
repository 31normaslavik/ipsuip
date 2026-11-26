// SPDX-License-Identifier: MIT
#pragma once

/** Синхронный HTTPS-клиент: GET по полному URL, внутренняя реализация на
 *  asio::awaitable с таймаутами, повторами и корректным TLS shutdown. **/

#include <cstdint>
#include <functional>
#include <string>

namespace ipsuip {

// Коллбэк прогресса загрузки. Вызывается из потока, выполняющего загрузку.
// downloaded — байт получено (тело), total — общий размер или 0 если неизвестен.
// Возврат false прерывает загрузку.
using ProgressCallback =
    std::function<bool(std::uint64_t downloaded, std::uint64_t total)>;

// Параметры запроса.
struct GetOptions {
  ProgressCallback progress;       // прогресс/отмена (nullptr — без прогресса)
  int max_retries = 3;             // число повторов при сетевых ошибках
  int connect_timeout_sec = 15;    // таймаут TCP-соединения
  int tls_timeout_sec = 15;        // таймаут TLS handshake
  int idle_timeout_sec = 60;       // таймаут простоя между чанками
};

// GET по полному https:// URL. Возвращает тело ответа.
// Бросает std::runtime_error / boost::beast::system_error при ошибке.
std::string get(const std::string &url);
std::string get(const std::string &url, const GetOptions &opts);

// GET с записью тела в файл по указанному пути (потоково, без хранения всего
// тела в памяти). Возвращает количество записанных байт.
std::uint64_t get_to_file(const std::string &url, const std::string &filepath,
                          const GetOptions &opts = {});

// Глобальная настройка прокси (применяется ко всем последующим запросам).
// raw: [scheme://][user:pass@]host:port. scheme: http, socks5, socks5h.
void set_proxy(const std::string &raw);

// Убирает глобальный прокси.
void clear_proxy();

// Возвращает текущий URL прокси (пустая строка если не установлен).
std::string proxy_url();

} // namespace ipsuip

