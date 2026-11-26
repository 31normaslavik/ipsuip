// SPDX-License-Identifier: MIT
#include "http_client_sync_ssl.h"
#include "proxy.h"
#include "root_certificates.h"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace ipsuip {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

static constexpr std::size_t kChunkSize = 65536; // 64 KB

// Глобальный прокси (thread-safe, read-only after set)
static std::atomic<const ProxyInfo *> g_proxy{nullptr};

void set_proxy_impl(const std::string &raw) {
  auto *info = new ProxyInfo(parse_proxy(raw));
  g_proxy.store(info, std::memory_order_release);
}

static const ProxyInfo *get_proxy() {
  return g_proxy.load(std::memory_order_acquire);
}

static void cleanup_proxy() {
  const ProxyInfo *p = g_proxy.exchange(nullptr);
  delete p;
}

struct UrlParts {
  std::string host;
  std::string port;
  std::string target; // path + query
};

UrlParts parse_url(const std::string &url) {
  const std::string scheme = "https://";
  if (url.rfind(scheme, 0) != 0)
    throw std::runtime_error("Only https:// URLs are supported: " + url);

  std::string rest = url.substr(scheme.size());
  UrlParts parts;
  parts.port = "443";

  auto slash = rest.find('/');
  std::string hostport =
      slash == std::string::npos ? rest : rest.substr(0, slash);
  parts.target = slash == std::string::npos ? "/" : rest.substr(slash);

  auto colon = hostport.rfind(':');
  if (colon != std::string::npos && hostport.find(']') == std::string::npos) {
    parts.host = hostport.substr(0, colon);
    parts.port = hostport.substr(colon + 1);
  } else {
    parts.host = hostport;
  }

  if (parts.host.empty())
    throw std::runtime_error("Cannot extract host from URL: " + url);

  return parts;
}

// HTTP CONNECT через прокси: возвращает поток после успешного туннелирования.
asio::awaitable<void> http_connect_proxy(beast::ssl_stream<beast::tcp_stream> &stream,
                                         tcp::resolver &resolver,
                                         const ProxyInfo &proxy,
                                         const std::string &target_host,
                                         const std::string &target_port,
                                         const GetOptions &opts) {
  auto &low = beast::get_lowest_layer(stream);
  auto ex = co_await asio::this_coro::executor;

  // Подключаемся к прокси
  low.expires_after(std::chrono::seconds(opts.connect_timeout_sec));
  auto results = co_await resolver.async_resolve(
      proxy.host, std::to_string(proxy.port), asio::use_awaitable);
  co_await low.async_connect(results, asio::use_awaitable);

  // Отправляем CONNECT запрос (без TLS, по обычному TCP)
  std::string connect_host = target_host + ":" + target_port;
  std::string connect_req = "CONNECT " + connect_host + " HTTP/1.1\r\n" +
                            "Host: " + connect_host + "\r\n";
  if (!proxy.user.empty() || !proxy.pass.empty()) {
    connect_req += "Proxy-Authorization: Basic " +
                   basic_auth(proxy.user, proxy.pass) + "\r\n";
  }
  connect_req += "\r\n";

  co_await asio::async_write(low, asio::buffer(connect_req),
                             asio::use_awaitable);

  // Читаем ответ прокси
  beast::flat_buffer buf;
  http::response<http::string_body> proxy_res;
  co_await http::async_read(low, buf, proxy_res, asio::use_awaitable);

  if (proxy_res.result_int() != 200)
    throw std::runtime_error("Proxy CONNECT failed: HTTP " +
                             std::to_string(proxy_res.result_int()));
}

// SOCKS5 CONNECT через прокси (RFC 1928/1929).
asio::awaitable<void> socks5_connect(beast::ssl_stream<beast::tcp_stream> &stream,
                                     tcp::resolver &resolver,
                                     const ProxyInfo &proxy,
                                     const std::string &target_host,
                                     const std::uint16_t target_port,
                                     const GetOptions &opts) {
  auto &low = beast::get_lowest_layer(stream);
  auto ex = co_await asio::this_coro::executor;

  // Подключаемся к SOCKS5-прокси
  low.expires_after(std::chrono::seconds(opts.connect_timeout_sec));
  auto results = co_await resolver.async_resolve(
      proxy.host, std::to_string(proxy.port), asio::use_awaitable);
  co_await low.async_connect(results, asio::use_awaitable);

  // Greeting: версия 5, методы
  std::array<unsigned char, 3> greeting;
  greeting[0] = 0x05; // версия
  bool need_auth = !proxy.user.empty() || !proxy.pass.empty();
  greeting[1] = need_auth ? 1 : 1; // число методов
  greeting[2] = need_auth ? 0x02 : 0x00; // 0x02 = username/password, 0x00 = no auth

  co_await asio::async_write(low, asio::buffer(greeting), asio::use_awaitable);

  std::array<unsigned char, 2> method_resp;
  co_await asio::async_read(low, asio::buffer(method_resp), asio::use_awaitable);

  if (method_resp[0] != 0x05)
    throw std::runtime_error("SOCKS5: invalid version in greeting response");

  if (method_resp[1] == 0xFF)
    throw std::runtime_error("SOCKS5: no acceptable auth method");

  // Аутентификация по логину/паролю (RFC 1929)
  if (method_resp[1] == 0x02) {
    if (proxy.user.size() > 255 || proxy.pass.size() > 255)
      throw std::runtime_error("SOCKS5: username/password too long");

    std::vector<unsigned char> auth_msg;
    auth_msg.push_back(0x01); // версия протокола аутентификации
    auth_msg.push_back(static_cast<unsigned char>(proxy.user.size()));
    auth_msg.insert(auth_msg.end(), proxy.user.begin(), proxy.user.end());
    auth_msg.push_back(static_cast<unsigned char>(proxy.pass.size()));
    auth_msg.insert(auth_msg.end(), proxy.pass.begin(), proxy.pass.end());

    co_await asio::async_write(low, asio::buffer(auth_msg), asio::use_awaitable);

    std::array<unsigned char, 2> auth_resp;
    co_await asio::async_read(low, asio::buffer(auth_resp), asio::use_awaitable);

    if (auth_resp[0] != 0x01 || auth_resp[1] != 0x00)
      throw std::runtime_error("SOCKS5: authentication failed");
  }

  // CONNECT запрос (именной адрес — DNS на стороне прокси)
  if (target_host.size() > 255)
    throw std::runtime_error("SOCKS5: hostname too long");

  std::vector<unsigned char> cmd;
  cmd.push_back(0x05); // версия
  cmd.push_back(0x01); // COMND = CONNECT
  cmd.push_back(0x00); // RSV
  cmd.push_back(0x03); // ATYP = доменное имя

  cmd.push_back(static_cast<unsigned char>(target_host.size()));
  cmd.insert(cmd.end(), target_host.begin(), target_host.end());
  cmd.push_back(static_cast<unsigned char>((target_port >> 8) & 0xFF));
  cmd.push_back(static_cast<unsigned char>(target_port & 0xFF));

  co_await asio::async_write(low, asio::buffer(cmd), asio::use_awaitable);

  // Читаем ответ: версия, ответ, RSV, ATYP, затем адрес
  std::array<unsigned char, 4> head;
  co_await asio::async_read(low, asio::buffer(head), asio::use_awaitable);

  if (head[0] != 0x05)
    throw std::runtime_error("SOCKS5: invalid version in CONNECT response");
  if (head[1] != 0x00) {
    std::string msg = "SOCKS5: CONNECT failed, reply code 0x" +
                      [&] {
                        std::string hex(2, '0');
                        const char *digits = "0123456789abcdef";
                        hex[0] = digits[(head[1] >> 4) & 0xF];
                        hex[1] = digits[head[1] & 0xF];
                        return hex;
                      }();
    throw std::runtime_error(msg);
  }

  // Пропускаем адрес привязки (зависит от ATYP)
  switch (head[3]) {
    case 0x01: { // IPv4
      std::array<unsigned char, 6> skip;
      co_await asio::async_read(low, asio::buffer(skip), asio::use_awaitable);
      break;
    }
    case 0x03: { // домен
      std::array<unsigned char, 1> len;
      co_await asio::async_read(low, asio::buffer(len), asio::use_awaitable);
      std::vector<unsigned char> skip(len[0] + 2);
      co_await asio::async_read(low, asio::buffer(skip), asio::use_awaitable);
      break;
    }
    case 0x04: { // IPv6
      std::array<unsigned char, 18> skip;
      co_await asio::async_read(low, asio::buffer(skip), asio::use_awaitable);
      break;
    }
    default:
      throw std::runtime_error("SOCKS5: unknown address type in reply");
  }
}

// Корутина: одна попытка GET. Тело возвращается или пишется в file_out.
asio::awaitable<std::string> try_get(const UrlParts &url,
                                     const GetOptions &opts,
                                     std::ofstream *file_out,
                                     std::uint64_t *out_bytes) {
  auto ex = co_await asio::this_coro::executor;

  asio::ssl::context ssl_ctx(asio::ssl::context::tls_client);
  load_root_certificates(ssl_ctx);
  ssl_ctx.set_verify_mode(asio::ssl::verify_peer);

  tcp::resolver resolver(ex);
  beast::ssl_stream<beast::tcp_stream> stream(ex, ssl_ctx);

  // SNI
  if (!SSL_set_tlsext_host_name(stream.native_handle(), url.host.c_str())) {
    beast::error_code ec{static_cast<int>(::ERR_get_error()),
                         asio::error::get_ssl_category()};
    throw beast::system_error{ec};
  }

  const ProxyInfo *proxy = get_proxy();

  if (proxy && proxy->scheme == ProxyScheme::Http) {
    co_await http_connect_proxy(stream, resolver, *proxy, url.host, url.port, opts);
  } else if (proxy && proxy->scheme == ProxyScheme::Socks5) {
    std::uint16_t tport = static_cast<std::uint16_t>(std::stoul(url.port));
    co_await socks5_connect(stream, resolver, *proxy, url.host, tport, opts);
  } else {
    auto &low = beast::get_lowest_layer(stream);
    low.expires_after(std::chrono::seconds(opts.connect_timeout_sec));
    auto results =
        co_await resolver.async_resolve(url.host, url.port, asio::use_awaitable);
    co_await low.async_connect(results, asio::use_awaitable);
  }

  // TLS handshake
  auto &low = beast::get_lowest_layer(stream);
  low.expires_after(std::chrono::seconds(opts.tls_timeout_sec));
  co_await stream.async_handshake(asio::ssl::stream_base::client,
                                  asio::use_awaitable);

  // Снимаем таймаут TLS-хендшейка перед запросом: expires_after(0) означал бы
  // «истечь немедленно» и закрыл бы сокет на первой же асинхронной операции.
  low.expires_never();

  // Send request
  http::request<http::empty_body> req(http::verb::get, url.target, 11);
  req.set(http::field::host, url.host);
  req.set(http::field::user_agent, "ipsuip/0.1");
  req.set(http::field::accept_encoding, "identity");

  co_await http::async_write(stream, req, asio::use_awaitable);

  // Read response header
  http::response_parser<http::buffer_body> parser;
  parser.body_limit((std::numeric_limits<std::uint64_t>::max)());

  beast::flat_buffer buf;
  co_await http::async_read_header(stream, buf, parser, asio::use_awaitable);

  auto &res = parser.get();
  unsigned status = res.result_int();
  if (status < 200 || status >= 300)
    throw std::runtime_error("HTTP " + std::to_string(status) + " for " +
                             url.host + url.target);

  std::uint64_t content_length = 0;
  if (auto it = res.find(http::field::content_length); it != res.end())
    content_length = std::stoull(std::string(it->value()));

  std::uint64_t downloaded = 0;

  std::string body;
  if (content_length > 0 && !file_out)
    body.reserve(static_cast<std::size_t>(content_length));

  // Stream body chunks using synchronous parser.get()
  // (acceptable: single-threaded io_context, nothing else to run)
  std::array<char, kChunkSize> chunk{};
  beast::error_code ec;

  for (;;) {
    res.body().data = chunk.data();
    res.body().size = chunk.size();

    low.expires_after(std::chrono::seconds(opts.idle_timeout_sec));

    std::size_t n = http::read(stream, buf, parser, ec);
    (void)n;

    if (ec && ec != http::error::need_buffer) {
      if (ec == asio::error::timed_out)
        throw beast::system_error(asio::error::timed_out, "idle timeout");
      throw beast::system_error{ec};
    }

    std::size_t got = chunk.size() - res.body().size;
    if (got > 0) {
      downloaded += got;
      if (file_out) {
        file_out->write(chunk.data(), static_cast<std::streamsize>(got));
        if (!*file_out)
          throw std::runtime_error("Write error to output file");
      } else {
        body.append(chunk.data(), got);
      }

      if (opts.progress && !opts.progress(downloaded, content_length))
        throw std::runtime_error("Download cancelled");
    }

    if (parser.is_done()) break;
  }

  // Graceful TLS shutdown (ignore errors)
  low.expires_after(std::chrono::seconds(5));
  beast::error_code shutdown_ec;
  stream.shutdown(shutdown_ec);
  (void)shutdown_ec;

  if (file_out) {
    file_out->flush();
    if (out_bytes) *out_bytes = downloaded;
    co_return std::string{};
  }
  co_return body;
}

// Запускает корутину на io_context с повторами и backoff.
std::string run_get(const UrlParts &url, const GetOptions &opts,
                    std::ofstream *file_out, std::uint64_t *out_bytes,
                    const std::string *filepath = nullptr) {
  int attempts = opts.max_retries + 1;

  for (int i = 0; i < attempts; ++i) {
    try {
      asio::io_context ioc;
      std::exception_ptr eptr;
      std::string body;

      asio::co_spawn(
          ioc, try_get(url, opts, file_out, out_bytes),
          [&](std::exception_ptr ep, std::string b) {
            eptr = ep;
            body = std::move(b);
          });

      ioc.run();

      if (eptr) std::rethrow_exception(eptr);
      return body;

    } catch (...) {
      if (i + 1 >= attempts) throw;
      // Повтор начинается с чистого файла, иначе данные ответа склеятся.
      if (file_out) {
        if (filepath) {
          file_out->close();
          file_out->clear();
          file_out->open(*filepath, std::ios::binary | std::ios::trunc);
          if (!*file_out)
            throw std::runtime_error("Cannot reopen file for retry: " + *filepath);
        } else {
          file_out->clear();
          file_out->seekp(0);
        }
      }
      if (out_bytes) *out_bytes = 0;
      std::this_thread::sleep_for(std::chrono::seconds(1 << i));
    }
  }
  return {};
}

} // namespace

std::string get(const std::string &url) { return get(url, GetOptions{}); }

std::string get(const std::string &url, const GetOptions &opts) {
  UrlParts parts = parse_url(url);
  return run_get(parts, opts, nullptr, nullptr);
}

std::uint64_t get_to_file(const std::string &url, const std::string &filepath,
                          const GetOptions &opts) {
  UrlParts parts = parse_url(url);

  std::ofstream file(filepath, std::ios::binary | std::ios::trunc);
  if (!file)
    throw std::runtime_error("Cannot open file for writing: " + filepath);

  std::uint64_t bytes = 0;
  run_get(parts, opts, &file, &bytes, &filepath);
  file.close();
  return bytes;
}

void set_proxy(const std::string &raw) { set_proxy_impl(raw); }

void clear_proxy() { cleanup_proxy(); }

std::string proxy_url() {
  const ProxyInfo *p = get_proxy();
  if (!p || p->scheme == ProxyScheme::None) return std::string();
  std::string s;
  switch (p->scheme) {
    case ProxyScheme::Http:   s = "http://"; break;
    case ProxyScheme::Socks5: s = "socks5://"; break;
    default: break;
  }
  if (!p->user.empty() || !p->pass.empty()) {
    s += p->user + ":" + p->pass + "@";
  }
  s += p->host + ":" + std::to_string(p->port);
  return s;
}

} // namespace ipsuip
