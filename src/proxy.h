// SPDX-License-Identifier: MIT
#pragma once

/** Парсер и проверка строки прокси. **/

#include <cstdint>
#include <string>

namespace ipsuip {

enum class ProxyScheme { None, Http, Socks5 };

struct ProxyInfo {
  ProxyScheme scheme = ProxyScheme::None;
  std::string host;
  std::uint16_t port = 0;
  std::string user;
  std::string pass;
};

// Парсит "[scheme://][user:pass@]host[:port]".
// scheme: http, https, socks5, socks5h (socks5h ≡ socks5); без схемы — http.
// Порт по умолчанию: 8080 для http(s), 1080 для socks5.
// Пустая строка → scheme=None. При ошибке бросает std::invalid_argument.
ProxyInfo parse_proxy(const std::string &raw);

// Проверяет строку, не бросая исключение.
bool is_valid_proxy(const std::string &raw, std::string *error = nullptr);

// Базовая авторизация (RFC 7617).
std::string basic_auth(const std::string &user, const std::string &pass);

} // namespace ipsuip
