#include "paths.h"

#include <cstdlib>

namespace ipsuip::paths {

std::filesystem::path home_dir() {
#if defined(_WIN32)
  if (const char *home = std::getenv("USERPROFILE"); home && *home)
    return std::filesystem::path(home);
  if (const char *home = std::getenv("HOME"); home && *home)
    return std::filesystem::path(home);
  return std::filesystem::current_path();
#else
  if (const char *home = std::getenv("HOME"); home && *home)
    return std::filesystem::path(home);
  return std::filesystem::current_path();
#endif
}

std::filesystem::path cache_dir() {
#if defined(_WIN32)
  if (const char *local = std::getenv("LOCALAPPDATA"); local && *local)
    return std::filesystem::path(local) / "ipsuip" / "data.json";
  // fallback — home dir
  return home_dir() / "AppData" / "Local" / "ipsuip" / "data.json";
#else
  if (const char *xdg = std::getenv("XDG_CACHE_HOME"); xdg && *xdg)
    return std::filesystem::path(xdg) / "ipsuip" / "data.json";
  return home_dir() / ".cache" / "ipsuip" / "data.json";
#endif
}

std::filesystem::path config_dir() {
#if defined(_WIN32)
  if (const char *app = std::getenv("APPDATA"); app && *app)
    return std::filesystem::path(app) / "ipsuip";
  return home_dir() / "AppData" / "Roaming" / "ipsuip";
#else
  if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
    return std::filesystem::path(xdg) / "ipsuip";
  return home_dir() / ".config" / "ipsuip";
#endif
}

} // namespace ipsuip::paths
