// SPDX-License-Identifier: MIT
#include "config.h"
#include "paths.h"

#include <boost/json.hpp>

#include <fstream>
#include <system_error>

namespace ipsuip {

std::filesystem::path default_config_path() {
  return paths::config_dir() / "config.json";
}

Config load_config() {
  Config cfg;
  const std::filesystem::path path = default_config_path();

  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) return cfg;

  std::string text((std::istreambuf_iterator<char>(ifs)),
                    std::istreambuf_iterator<char>());

  boost::system::error_code ec;
  const boost::json::value root = boost::json::parse(text, ec);
  if (ec || !root.is_object()) return cfg;

  const boost::json::value *proxy = root.as_object().if_contains("proxy");
  if (proxy && proxy->is_string())
    cfg.proxy = std::string(proxy->as_string().c_str());

  const boost::json::value *subdirs = root.as_object().if_contains("create_subdirs");
  if (subdirs && subdirs->is_bool())
    cfg.create_subdirs = subdirs->as_bool();

  return cfg;
}

bool save_config(const Config &cfg) {
  boost::json::object root;
  root["proxy"] = cfg.proxy;
  root["create_subdirs"] = cfg.create_subdirs;

  const std::string text = boost::json::serialize(root);

  std::error_code ec;
  const std::filesystem::path path = default_config_path();
  const std::filesystem::path dir = path.parent_path();
  if (!dir.empty()) {
    std::filesystem::create_directories(dir, ec);
    if (ec) return false;
  }

  // Атомарная запись.
  std::filesystem::path tmp = path;
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

  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    return false;
  }
  return true;
}

} // namespace ipsuip
