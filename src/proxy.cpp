#include "proxy.h"

#include <cctype>
#include <limits>
#include <stdexcept>

namespace ipsuip {
namespace {

std::string lower(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

int hex_value(unsigned char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::string url_decode(const std::string &s) {
  std::string out;
  out.reserve(s.size());

  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size()) {
      const int hi = hex_value(static_cast<unsigned char>(s[i + 1]));
      const int lo = hex_value(static_cast<unsigned char>(s[i + 2]));
      if (hi >= 0 && lo >= 0) {
        out += static_cast<char>((hi << 4) | lo);
        i += 2;
        continue;
      }
    }
    out += s[i];
  }
  return out;
}

std::uint16_t parse_port(const std::string &text) {
  if (text.empty()) throw std::invalid_argument("Не указан порт прокси");
  unsigned long value = 0;
  for (char c : text) {
    if (!std::isdigit(static_cast<unsigned char>(c)))
      throw std::invalid_argument("Некорректный порт прокси: " + text);
    value = value * 10 + static_cast<unsigned>(c - '0');
    if (value > std::numeric_limits<std::uint16_t>::max())
      throw std::invalid_argument("Порт прокси вне диапазона 1..65535: " + text);
  }
  if (value == 0)
    throw std::invalid_argument("Порт прокси вне диапазона 1..65535: " + text);
  return static_cast<std::uint16_t>(value);
}

} // namespace

ProxyInfo parse_proxy(const std::string &raw) {
  ProxyInfo info;
  std::string s = raw;
  const auto first = s.find_first_not_of(" \t\r\n");
  const auto last = s.find_last_not_of(" \t\r\n");
  if (first == std::string::npos) return info;
  s = s.substr(first, last - first + 1);

  std::string scheme;
  const auto proto = s.find("://");
  if (proto != std::string::npos) {
    scheme = lower(s.substr(0, proto));
    s = s.substr(proto + 3);
  }

  if (scheme.empty() || scheme == "http" || scheme == "https") {
    info.scheme = ProxyScheme::Http;
  } else if (scheme == "socks5" || scheme == "socks5h") {
    info.scheme = ProxyScheme::Socks5;
  } else {
    throw std::invalid_argument("Неизвестная схема прокси: " + scheme);
  }

  // Учётные данные (правый @ — пароль тоже может содержать @).
  const auto at = s.rfind('@');
  if (at != std::string::npos) {
    const std::string auth = s.substr(0, at);
    s = s.substr(at + 1);
    const auto colon = auth.find(':');
    info.user = url_decode(colon == std::string::npos ? auth : auth.substr(0, colon));
    if (colon != std::string::npos) info.pass = url_decode(auth.substr(colon + 1));
  }

  // host[:port], с поддержкой [IPv6]:port.
  std::string host;
  std::string port;
  if (!s.empty() && s[0] == '[') {
    const auto close = s.find(']');
    if (close == std::string::npos)
      throw std::invalid_argument("Незакрытая скобка в IPv6-адресе прокси");
    host = s.substr(1, close - 1);
    s = s.substr(close + 1);
    if (!s.empty()) {
      if (s[0] != ':') throw std::invalid_argument("Ожидался ':PORT' после IPv6-адреса");
      port = s.substr(1);
    }
  } else {
    const auto colon = s.rfind(':');
    if (colon != std::string::npos) {
      host = s.substr(0, colon);
      port = s.substr(colon + 1);
      if (host.find(':') != std::string::npos)
        throw std::invalid_argument("IPv6-адрес прокси нужно заключить в []");
    } else {
      host = s;
    }
  }

  if (host.empty())
    throw std::invalid_argument("Не указан хост прокси");
  if (host.find_first_of(" \t/") != std::string::npos)
    throw std::invalid_argument("Некорректный хост прокси: " + host);

  info.host = host;
  info.port = port.empty()
      ? (info.scheme == ProxyScheme::Socks5 ? 1080 : 8080)
      : parse_port(port);
  return info;
}

bool is_valid_proxy(const std::string &raw, std::string *error) {
  try {
    parse_proxy(raw);
    if (error) error->clear();
    return true;
  } catch (const std::exception &e) {
    if (error) *error = e.what();
    return false;
  }
}

std::string basic_auth(const std::string &user, const std::string &pass) {
  static const char table[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const std::string raw = user + ":" + pass;
  std::string out;

  for (std::size_t i = 0; i < raw.size(); i += 3) {
    const unsigned char b0 = static_cast<unsigned char>(raw[i]);
    const unsigned char b1 = i + 1 < raw.size()
        ? static_cast<unsigned char>(raw[i + 1]) : 0;
    const unsigned char b2 = i + 2 < raw.size()
        ? static_cast<unsigned char>(raw[i + 2]) : 0;

    out += table[b0 >> 2];
    out += table[((b0 & 0x03) << 4) | (b1 >> 4)];
    out += i + 1 < raw.size() ? table[((b1 & 0x0F) << 2) | (b2 >> 6)] : '=';
    out += i + 2 < raw.size() ? table[b2 & 0x3F] : '=';
  }
  return out;
}

} // namespace ipsuip
