# Usage

Русская версия: [USAGE.md](USAGE.md)

## CLI

```bash
# List continents
ipsuip --mainland

# Countries and cities
ipsuip --mainland EU --country
ipsuip --mainland EU --country RU --province Chelyabinsk --city

# Save ranges for the selected city to a directory
ipsuip --mainland EU --country RU --province Chelyabinsk --city Kartaly \
  --output ./out

# Refresh cache, hide progress, set or disable proxy
ipsuip --mainland EU --refresh
ipsuip --mainland EU --quiet
ipsuip --mainland EU --proxy http://user:pass@proxy.example:8080
ipsuip --mainland EU --no-proxy
```

`--refresh` reloads navigation pages, `--quiet` hides the progress indicator.
Progress is shown only when output is a terminal.

## TUI

```bash
ipsuip --tui
```

| Key | Action |
|---|---|
| `↑`/`↓`, `k`/`j` | select |
| `Enter` | open / save |
| `Backspace`, `Esc`, `←` | back |
| `s` (`F2`) | settings |
| `q` | quit |

In the "Settings" menu, you can clear the cache and set a proxy. The proxy is
applied immediately and saved to the config; an empty string disables it.

## Cache and config

| | Linux | Windows |
|---|---|---|
| Cache | `$XDG_CACHE_HOME/ipsuip/data.json` | `%LOCALAPPDATA%\ipsuip\data.json` |
| Config | `$XDG_CONFIG_HOME/ipsuip/config.json` | `%APPDATA%\ipsuip\config.json` |

If XDG variables are not set, `~/.cache` and `~/.config` are used.

Example `config.json`:

```json
{
  "proxy": "socks5://127.0.0.1:9050",
  "create_subdirs": false
}
```

Supported: `http://`, `https://`, `socks5://`, `socks5h://`, `host:port`,
login/password, and IPv6. Priority: `--proxy` > config; `--no-proxy` disables
the proxy for the current run.
