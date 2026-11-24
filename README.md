# ipsuip

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

## Зависимости

Для сборки нужны компилятор C++20, CMake 3.28+, Ninja или Make и OpenSSL
с заголовками. `lexbor`, `Boost` и `PDCursesMod` CMake загрузит и соберёт сам.

OpenSSL используется как системная библиотека:

- для сборки нужны заголовки: в Arch — пакет `openssl`, в Debian/Ubuntu —
  `libssl-dev`;
- для запуска нужны `libssl`/`libcrypto`: в Arch — `openssl`, в Debian/Ubuntu —
  `libssl3` или `libssl3t64`, в Windows — DLL в `PATH` или рядом с
  `ipsuip.exe`;
- готовые пакеты не включают OpenSSL: зависимость ставит менеджер пакетов.

Проверить после сборки: `ldd ./build/ipsuip | grep -E 'ssl|crypto'`.

## Сборка и установка

### Linux

```bash
sudo pacman -S base-devel cmake ninja openssl
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/ipsuip --help
sudo cmake --install build --prefix /usr/local
```

По умолчанию включена опция `IPSUIP_STATIC=ON`: зависимости проекта линкуются
статически, а системные libc и OpenSSL остаются динамическими. Если нужна
обычная динамическая сборка, используйте `-DIPSUIP_STATIC=OFF`.

### Windows

Нужен OpenSSL SDK с заголовками и библиотеками; DLL должны лежать рядом с
`ipsuip.exe` или быть в `PATH`.

MSVC:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

MinGW-w64 (включая кросс-сборку с Linux):

```bash
cmake -S . -B build-mingw -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw
```

Для кросс-сборки нужен OpenSSL для mingw-w64 (в Arch — `mingw-w64-openssl`
из AUR).

## Пакеты

```bash
cd build
cpack -G TGZ
cpack -G DEB
cpack -G ZIP
cpack -G NSIS
```

На Linux создаются TGZ и DEB, на Windows — ZIP и NSIS. В пакет входят
`ipsuip` и `README.md`; OpenSSL ставится отдельно. Для Arch Linux есть
`PKGBUILD`: `makepkg -si`.

## Использование

### CLI

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

### TUI

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
  "proxy": "socks5://127.0.0.1:9050"
}
```

Поддерживаются `http://`, `https://`, `socks5://`, `socks5h://`, `host:port`,
логин/пароль и IPv6. Приоритет: `--proxy` > конфиг; `--no-proxy` отключает
прокси для текущего запуска.
