#include "suip_cache.h"

// Boost.JSON в header-only режиме: реализация подключается ровно в одном
// модуле проекта (здесь).
#include <boost/json/src.hpp>

#include <boost/json.hpp>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace ipsuip {
namespace {

constexpr int kFormatVersion = 2;

std::filesystem::path home_dir() {
  if (const char *home = std::getenv("HOME"); home && *home)
    return std::filesystem::path(home);
  return std::filesystem::current_path();
}

boost::json::object target_to_json(const Target &t) {
  boost::json::object o;
  o["level"] = level_name(t.level);
  o["code"] = t.code;
  o["location"] = t.location;
  o["address"] = t.address;
  o["download"] = t.all_ip_direct_download;
  return o;
}

std::string json_string(const boost::json::object &o, const char *key) {
  const boost::json::value *v = o.if_contains(key);
  if (!v || !v->is_string()) return {};
  return std::string(v->as_string().c_str());
}

bool target_from_json(const boost::json::value &v, Target &t) {
  if (!v.is_object()) return false;
  const boost::json::object &o = v.as_object();

  TargetLevel level;
  if (!level_from_name(json_string(o, "level"), level)) return false;

  t.level = level;
  t.code = json_string(o, "code");
  t.location = json_string(o, "location");
  t.address = json_string(o, "address");
  t.all_ip_direct_download = json_string(o, "download");
  return !t.address.empty();
}

} // namespace

std::filesystem::path default_cache_path() {
  if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
    return std::filesystem::path(xdg) / "suipbiz" / "data.json";
  return home_dir() / ".config" / "suipbiz" / "data.json";
}

NavCache::NavCache() : path_(default_cache_path()) { load(); }

NavCache::NavCache(std::filesystem::path path) : path_(std::move(path)) {
  load();
}

const std::vector<Target> *NavCache::find(const std::string &url) const {
  auto it = pages_.find(url);
  return it == pages_.end() ? nullptr : &it->second;
}

void NavCache::store(const std::string &url,
                     const std::vector<Target> &targets) {
  if (url.empty() || targets.empty()) return;

  // Список из одной собственной ссылки страницы — это лист иерархии
  // (страница с IP-адресами): такие страницы не кэшируем.
  if (targets.size() == 1 && targets.front().address == url) return;

  pages_[url] = targets;
}

bool NavCache::load() {
  pages_.clear();

  std::ifstream ifs(path_, std::ios::binary);
  if (!ifs) return false;

  std::ostringstream buf;
  buf << ifs.rdbuf();
  const std::string text = buf.str();
  if (text.empty()) return false;

  boost::system::error_code ec;
  const boost::json::value root = boost::json::parse(text, ec);
  if (ec || !root.is_object()) return false;

  // Файл другой версии формата считаем недействительным.
  const boost::json::value *version = root.as_object().if_contains("version");
  if (!version || !version->is_int64() ||
      version->as_int64() != kFormatVersion)
    return false;

  const boost::json::value *pages = root.as_object().if_contains("pages");
  if (!pages || !pages->is_object()) return false;

  for (const auto &entry : pages->as_object()) {
    if (!entry.value().is_array()) continue;

    std::vector<Target> targets;
    for (const auto &item : entry.value().as_array()) {
      Target t;
      if (target_from_json(item, t)) targets.push_back(std::move(t));
    }
    if (!targets.empty())
      pages_.emplace(std::string(entry.key().data(), entry.key().size()),
                     std::move(targets));
  }
  return true;
}

bool NavCache::save() const {
  boost::json::object pages;
  for (const auto &[url, targets] : pages_) {
    boost::json::array arr;
    for (const auto &t : targets) arr.push_back(target_to_json(t));
    pages[url] = std::move(arr);
  }

  boost::json::object root;
  root["version"] = kFormatVersion;
  root["pages"] = std::move(pages);
  const std::string text = boost::json::serialize(root);

  std::error_code ec;
  const std::filesystem::path dir = path_.parent_path();
  if (!dir.empty()) {
    std::filesystem::create_directories(dir, ec);
    if (ec) return false;
  }

  // Атомарная запись: сначала во временный файл, затем rename.
  std::filesystem::path tmp = path_;
  tmp += ".tmp";
  {
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    ofs << text;
    ofs.flush();
    if (!ofs) {
      ofs.close();
      std::filesystem::remove(tmp, ec);
      return false;
    }
  }

  std::filesystem::rename(tmp, path_, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    return false;
  }
  return true;
}

} // namespace ipsuip
