# ipsuip

[![CI](https://github.com/31normaslavik/ipsuip/actions/workflows/ci.yml/badge.svg)](https://github.com/31normaslavik/ipsuip/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/31normaslavik/ipsuip)](https://github.com/31normaslavik/ipsuip/releases)

English version: [README_EN.md](README_EN.md)

`ipsuip` — утилита для парсинга IP-адресов с [suip.biz](https://suip.biz/).
Она помогает найти диапазоны адресов нужной страны или города и сохранить их
в форматах Range или CIDR.

Работает в командной строке и в интерактивном меню.

## Возможности

- навигация: континент → страна → регион → город;
- получение IP-диапазонов и сохранение в Range/CIDR;
- CLI и TUI;
- настройка HTTP/SOCKS5-прокси;
- индикатор прогресса загрузки;
- кэш навигации и его очистка.

## Быстрый старт

Готовые архивы и пакеты — в [GitHub Releases](https://github.com/31normaslavik/ipsuip/releases).
Для Arch Linux доступны пакеты AUR `ipsuip` (по тегу) и `ipsuip-git` (ветка `main`).

Сборка из исходников (Linux, CMake 3.28+, C++20, Ninja, OpenSSL с заголовками):

```bash
cmake --preset unix
cmake --build --preset unix
./build/unix/ipsuip --help
```

Примеры использования:

```bash
ipsuip --mainland
ipsuip --mainland EU --country RU --province Chelyabinsk --city Kartaly --output ./out
ipsuip --tui
```

## Документация

- [Сборка, зависимости и пакеты](docs/BUILD.md)
- [Использование: CLI, TUI, кэш и конфиг](docs/USAGE.md)
- [Участие в разработке](CONTRIBUTING.md)
- [Релизный чек-лист](docs/RELEASING.md)
- [Сообщение об уязвимости](SECURITY.md)
- [История изменений](CHANGELOG.md)

## Лицензия

`ipsuip` распространяется под лицензией [MIT](LICENSE). Лицензии зависимостей
и атрибуция стороннего кода перечислены в
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md); полные тексты — в
[`LICENSES/`](LICENSES/).
