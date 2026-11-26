// SPDX-License-Identifier: MIT
#include "config.h"
#include "suip_cache.h"
#include "test_util.h"

#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#define IPSUIP_TEST_PID _getpid()
#else
#include <unistd.h>
#define IPSUIP_TEST_PID getpid()
#endif

namespace fs = std::filesystem;

namespace {

void set_env(const char *name, const std::string &value) {
#if defined(_WIN32)
  _putenv_s(name, value.c_str());
#else
  setenv(name, value.c_str(), 1);
#endif
}

} // namespace

int main() {
  const fs::path tmp =
      fs::temp_directory_path() / ("ipsuip-test-" + std::to_string(IPSUIP_TEST_PID));
  fs::create_directories(tmp);

  set_env("XDG_CONFIG_HOME", tmp.string());
  set_env("APPDATA", tmp.string());

  ipsuip::Config cfg;
  cfg.proxy = "socks5://127.0.0.1:9050";
  cfg.create_subdirs = true;
  CHECK(ipsuip::save_config(cfg));

  const ipsuip::Config loaded = ipsuip::load_config();
  CHECK_EQ(loaded.proxy, std::string("socks5://127.0.0.1:9050"));
  CHECK(loaded.create_subdirs);

  // Файл конфига может содержать пароль прокси: он не должен быть
  // доступен другим пользователям системы.
#if !defined(_WIN32)
  const auto mode = fs::status(ipsuip::default_config_path()).permissions();
  CHECK((mode & fs::perms::group_read) == fs::perms::none);
  CHECK((mode & fs::perms::others_read) == fs::perms::none);
#endif

  const fs::path cache_file = tmp / "data.json";
  Target t;
  t.level = TargetLevel::Continent;
  t.code = "EU";
  t.location = "Europe";
  t.address = "https://suip.biz/?act=all-country-ip&continent=EU";

  Target asia;
  asia.level = TargetLevel::Continent;
  asia.code = "AS";
  asia.location = "Asia";
  asia.address = "https://suip.biz/?act=all-country-ip&continent=AS";

  ipsuip::NavCache cache(cache_file);
  // Одна ссылка на саму страницу трактуется как лист и в кэш не попадает.
  cache.store(t.address, {t, asia});
  CHECK(cache.save());

  ipsuip::NavCache reopened(cache_file);
  CHECK(reopened.find(t.address) != nullptr);
  if (reopened.find(t.address) != nullptr)
    CHECK_EQ(reopened.find(t.address)->front().location, std::string("Europe"));

  reopened.clear();
  CHECK(!fs::exists(cache_file));
  fs::remove_all(tmp);

  TEST_REPORT("config_cache");
}
