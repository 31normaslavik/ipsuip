#include "ip_convert.h"

#include <cstdint>

namespace {

bool parse_ipv4(const std::string &s, std::uint32_t &out) {
  std::uint32_t value = 0;
  int parts = 0;
  size_t start = 0;
  while (start <= s.size()) {
    size_t end = s.find('.', start);
    if (end == std::string::npos) end = s.size();

    std::string part = s.substr(start, end - start);
    if (part.empty() || part.size() > 3) return false;

    unsigned num = 0;
    for (char c : part) {
      if (c < '0' || c > '9') return false;
      num = num * 10 + static_cast<unsigned>(c - '0');
    }
    if (num > 255) return false;

    value = (value << 8) | num;
    ++parts;

    if (end == s.size()) break;
    start = end + 1;
  }

  if (parts != 4) return false;
  out = value;
  return true;
}

std::string to_dotted(std::uint32_t v) {
  return std::to_string((v >> 24) & 0xFFu) + "." +
         std::to_string((v >> 16) & 0xFFu) + "." +
         std::to_string((v >> 8) & 0xFFu) + "." +
         std::to_string(v & 0xFFu);
}

} // namespace

namespace ipsuip {

std::vector<std::string> range_to_cidr(const std::string &range) {
  std::vector<std::string> out;

  size_t dash = range.find('-');
  if (dash == std::string::npos || range.find('-', dash + 1) != std::string::npos)
    return out;

  std::uint32_t first = 0;
  std::uint32_t last = 0;
  if (!parse_ipv4(range.substr(0, dash), first) ||
      !parse_ipv4(range.substr(dash + 1), last) || first > last)
    return out;

  while (first <= last) {
    // Ищем самый крупный блок, который выровнен по first и не выходит за last:
    // перебираем длину префикса от /0 к /32.
    int prefix = 32;
    for (int p = 0; p <= 32; ++p) {
      const std::uint64_t block = std::uint64_t(1) << (32 - p);
      if ((first & (block - 1)) != 0) continue;                    // не выровнен
      if (static_cast<std::uint64_t>(first) + block - 1 > last) continue; // не влезает
      prefix = p;
      break;
    }

    const std::uint64_t block = std::uint64_t(1) << (32 - prefix);
    out.push_back(to_dotted(first) + "/" + std::to_string(prefix));

    const std::uint64_t next = static_cast<std::uint64_t>(first) + block;
    if (next > 0xFFFFFFFFull) break;
    first = static_cast<std::uint32_t>(next);
  }

  return out;
}

std::vector<std::string>
ranges_to_cidr(const std::vector<std::string> &ranges) {
  std::vector<std::string> out;
  out.reserve(ranges.size());
  for (const auto &range : ranges) {
    const auto cidrs = range_to_cidr(range);
    if (cidrs.empty())
      out.push_back(range); // IPv6 и всё неподдержанное оставляем как есть
    else
      out.insert(out.end(), cidrs.begin(), cidrs.end());
  }
  return out;
}

std::string sanitize_filename(const std::string &name) {
  std::string out;
  out.reserve(name.size());

  for (char ch : name) {
    const unsigned char c = static_cast<unsigned char>(ch);
    // многобайтовый UTF-8 (c >= 0x80) сохраняем как есть
    const bool allowed = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                         (c >= 'a' && c <= 'z') || c == '.' || c == '-' ||
                         c == '_' || c >= 0x80;
    if (allowed) {
      out += ch;
    } else if (!out.empty() && out.back() != '_') {
      out += '_';
    }
  }

  // схлопываем повторы '_' (в том числе пришедшие из исходного имени)
  std::string collapsed;
  collapsed.reserve(out.size());
  for (char ch : out) {
    if (ch == '_' && !collapsed.empty() && collapsed.back() == '_') continue;
    collapsed += ch;
  }

  size_t b = collapsed.find_first_not_of("_.");
  if (b == std::string::npos) return "unnamed";
  size_t e = collapsed.find_last_not_of("_.");
  return collapsed.substr(b, e - b + 1);
}

} // namespace ipsuip
