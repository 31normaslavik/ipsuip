// SPDX-License-Identifier: MIT
#include "proxy.h"
#include "test_util.h"

#include <stdexcept>
#include <string>

int main() {
  using ipsuip::ProxyScheme;
  using ipsuip::basic_auth;
  using ipsuip::is_valid_proxy;
  using ipsuip::parse_proxy;

  const auto http = parse_proxy("http://user:p%40ss@proxy.example:3128");
  CHECK(http.scheme == ProxyScheme::Http);
  CHECK_EQ(http.host, std::string("proxy.example"));
  CHECK_EQ(http.port, static_cast<std::uint16_t>(3128));
  CHECK_EQ(http.user, std::string("user"));
  CHECK_EQ(http.pass, std::string("p@ss"));

  const auto socks = parse_proxy("socks5h://127.0.0.1");
  CHECK(socks.scheme == ProxyScheme::Socks5);
  CHECK_EQ(socks.port, static_cast<std::uint16_t>(1080));

  const auto ipv6 = parse_proxy("[::1]:9050");
  CHECK_EQ(ipv6.host, std::string("::1"));
  CHECK_EQ(ipv6.port, static_cast<std::uint16_t>(9050));

  CHECK(parse_proxy("").scheme == ProxyScheme::None);
  CHECK(is_valid_proxy("host:8080"));
  CHECK_EQ(basic_auth("user", "pass"), std::string("dXNlcjpwYXNz"));

  CHECK(!is_valid_proxy("ftp://proxy.example:21"));
  CHECK(!is_valid_proxy("host:0"));
  CHECK(!is_valid_proxy("host:70000"));
  CHECK(!is_valid_proxy("host:port"));
  CHECK(!is_valid_proxy("[::1"));

  bool threw = false;
  try {
    (void)parse_proxy("ftp://proxy.example:21");
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  CHECK(threw);

  TEST_REPORT("proxy");
}
