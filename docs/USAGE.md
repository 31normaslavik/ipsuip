# Использование

English version: [USAGE_EN.md](USAGE_EN.md)

## CLI

```bash
# Список континентов
ipsuip --mainland

# Страны и города
ipsuip --mainland EU --country
ipsuip --mainland EU --country RU --province Chelyabinsk --city

# Сохранить диапазоны выбранного города в каталог
ipsuip --mainland EU --country RU --province Chelyabinsk --city Kartaly \
  --output ./out

# Обновить кэш, убрать прогресс, задать или отключить прокси
ipsuip --mainland EU --refresh
ipsuip --mainland EU --quiet
ipsuip --mainland EU --proxy http://user:pass@proxy.example:8080
ipsuip --mainland EU --no-proxy
```

`--refresh` загружает навигационные страницы заново, `--quiet` убирает
индикатор прогресса. Прогресс показывается только при выводе в терминал.

## TUI

```bash
ipsuip --tui
```

| Клавиша | Действие |
|---|---|
| `↑`/`↓`, `k`/`j` | выбор |
| `Enter` | открыть / сохранить |
| `Backspace`, `Esc`, `←` | назад |
| `s` (`F2`) | настройки |
| `q` | выход |

В меню «Настройки» можно очистить кэш и задать прокси. Прокси применяется
сразу и сохраняется в конфиг; пустая строка отключает его.

## Кэш и конфиг

| | Linux | Windows |
|---|---|---|
| Кэш | `$XDG_CACHE_HOME/ipsuip/data.json` | `%LOCALAPPDATA%\ipsuip\data.json` |
| Конфиг | `$XDG_CONFIG_HOME/ipsuip/config.json` | `%APPDATA%\ipsuip\config.json` |

Если переменные XDG не заданы, используются `~/.cache` и `~/.config`.

Пример `config.json`:

```json
{
  "proxy": "socks5://127.0.0.1:9050",
  "create_subdirs": false
}
```

Поддерживаются `http://`, `https://`, `socks5://`, `socks5h://`, `host:port`,
логин/пароль и IPv6. Приоритет: `--proxy` > конфиг; `--no-proxy` отключает
прокси для текущего запуска.
