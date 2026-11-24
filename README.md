# ipsuip

`ipsuip` — консольная программа и TUI для просмотра географической иерархии
IP-адресов сервиса [suip.biz](https://suip.biz/) (`континент → страна →
регион → город`) и экспорта диапазонов в форматах Range и CIDR.

Программа обходит страницы `?act=all-country-ip`, разбирает их с помощью
lexbor, кэширует навигацию в JSON и скачивает большие выгрузки адресов
потоково во временный файл (без загрузки всего HTML в память).

## Возможности

- консольный режим с выбором уровня по коду или названию;
- интерактивный TUI: списки, breadcrumb, навигация, сохранение IP-списков;
- меню «Настройки»: очистка кэша и настройка прокси с немедленным применением;
- индикатор прогресса загрузки в `stderr` (байты, проценты при известном
  `Content-Length`, скорость) и в статусной строке TUI;
- HTTP- и SOCKS5-прокси с авторизацией, настройка через `--proxy`, конфиг или TUI;
- кэш навигационных страниц (страницы с IP-адресами не кэшируются);
- статическая сборка на Linux и Windows.

## Зависимости

lexbor, Boost и PDCursesMod загружает и собирает сам CMake (FetchContent) —
вручную их ставить не нужно. **OpenSSL подключается только как системная
разделяемая библиотека**: он не собирается и не линкуется статически.

| Компонент | Версия | Способ подключения |
|---|---|---|
| C++20 компилятор | GCC/Clang/MSVC | системный |
| CMake | ≥ 3.28 | системный |
| Ninja / Make | — | системный |
| lexbor | 3.0.1 | FetchContent, `LEXBOR_BUILD_SHARED=OFF` |
| Boost (asio, beast, json, system, config, container, intrusive, optional, smart_ptr, mp11, endian, winapi и include-замыкание) | 1.92.0 | FetchContent модулей Boost |
| PDCursesMod | 4.5.4 | FetchContent, порт `vt` (Unix) / `wincon` (Windows), без демо-целей |
| OpenSSL: заголовки + `libssl.so` / `libcrypto.so` | ≥ 1.1.1 | системный `find_package(OpenSSL)`, `OPENSSL_USE_STATIC_LIBS=FALSE` |

Дополнительно определено `BOOST_ASIO_DISABLE_BOOST_DATE_TIME`: Asio работает
без Boost.DateTime, который не входит в замыкание заголовков.

### Где требуется OpenSSL

- **Для сборки:** нужны заголовки и разделяемые библиотеки OpenSSL (в Arch —
  пакет `openssl`, в Debian/Ubuntu — `libssl-dev`). `find_package(OpenSSL)`
  вызывается до настройки целей; `OPENSSL_USE_STATIC_LIBS` принудительно
  выключен, поэтому выбираются именно `.so`/`.dll.a`, а не `.a`.
- **Для запуска:** нужны системные `libssl.so` и `libcrypto.so` с compatible
  SONAME (`libssl.so.3`/`.so.4` и `libcrypto.so.3`/`.so.4` — зависит от
  версии дистрибутива). Arch: пакет `openssl`; Debian/Ubuntu: `libssl3` или
  `libssl3t64`; Windows: `libssl-3-x64.dll` и `libcrypto-3-x64.dll` рядом с
  `ipsuip.exe` или в `PATH`. Пакеты CPack и `PKGBUILD` объявляют эту
  зависимость, но сами `.so` не включают: их ставит менеджер пакетов или
  дистрибутив OpenSSL.
- **Проверка после сборки:**
  `ldd ./build/ipsuip | grep -E 'ssl|crypto'` — должны быть `libssl.so` и
  `libcrypto.so` (не `not found`).
- Если OpenSSL отсутствует, CMake завершится с ошибкой `Could NOT find
  OpenSSL` — это означает, что не установлены именно dev-файлы OpenSSL.
  Проверка находится в `CMakeLists.txt` в блоке «OpenSSL», до загрузки
  `FetchContent`-зависимостей.

## Сборка на Linux

```bash
sudo pacman -S base-devel cmake ninja openssl   # пример для Arch Linux
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/ipsuip --help
```

Установка:

```bash
sudo cmake --install build --prefix /usr/local
```

### Статическая сборка (кроме OpenSSL)

По умолчанию включена опция `IPSUIP_STATIC=ON`:

- Unix/MinGW: `-static-libgcc -static-libstdc++`. Флага `-static` **нет**:
  libc и OpenSSL остаются динамическими;
- MSVC: `/MT` (в Debug — `/MTd`, задаётся через `CMAKE_MSVC_RUNTIME_LIBRARY`);
- lexbor, Boost и PDCursesMod собираются и линкуются статически;
- OpenSSL — системный shared, в бинарник не включается.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DIPSUIP_STATIC=ON
cmake --build build
file build/ipsuip                       # dynamically linked
ldd build/ipsuip | grep -E 'ssl|crypto' # libssl.so / libcrypto.so
```

Статические архивы libc (`libc.a` и др.) больше не нужны, а вместе с ними
исчезает проблема `getaddrinfo`/NSS при полностью статической линковке.

## Сборка на Windows

### MSVC (Visual Studio)

Нужен OpenSSL SDK для Windows (например, Shining Light Productions) с
`libssl.lib` / `libcrypto.lib` и соответствующими DLL. DLL должны лежать рядом
с `ipsuip.exe` или быть в `PATH`.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Runtime-библиотека MSVC принудительно статическая (`/MT`).

### MinGW-w64 (в том числе кросс-сборка с Linux)

Нужен OpenSSL для mingw-w64: заголовки и импортные библиотеки
(`libssl.dll.a`, `libcrypto.dll.a`) в префиксе компилятора, а также рантайм-
DLL. В Arch для кросс-сборки можно использовать пакет `mingw-w64-openssl`
из AUR.

```bash
cmake -S . -B build-mingw -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw
```

Сборка использует порт PDCursesMod `wincon`; OpenSSL находится через
`find_package(OpenSSL)` в системном префиксе mingw-w64.

## Пакеты (CPack)

```bash
cd build
cpack -G TGZ      # Linux
cpack -G DEB      # Linux (пакет .deb)
cpack -G ZIP      # Windows
cpack -G NSIS     # Windows (установщик)
```

`CPACK_GENERATOR` автоматически выбирается по платформе: `TGZ;DEB` на Linux и
`ZIP;NSIS` на Windows. В пакет входят `ipsuip` и `README.md`. OpenSSL в пакет
**не включается**: DEB-пакет объявляет зависимость `libssl3 | libssl3t64`,
ZIP/NSIS ожидают OpenSSL DLL в системе.

Для Arch Linux есть `PKGBUILD` (AUR):

```bash
makepkg -si
```

## Использование

### CLI

```bash
# Список континентов
ipsuip --mainland

# Страны Европы / регионы России / города региона
ipsuip --mainland EU --country
ipsuip --mainland EU --country RU --province
ipsuip --mainland EU --country RU --province Chelyabinsk --city

# Диапазоны выбранного города
ipsuip --mainland EU --country RU --province Chelyabinsk --city Kartaly

# Сохранить диапазоны и CIDR выбранной цели в каталог
ipsuip --mainland EU --country RU --province Chelyabinsk --city Kartaly \
  --output ./out

# Обновить кэш навигации, отключить прогресс, задать прокси
ipsuip --mainland EU --refresh
ipsuip --mainland EU --quiet
ipsuip --mainland EU --proxy http://user:pass@proxy.example:8080
ipsuip --mainland EU --no-proxy
```

`--refresh` заставляет заново загрузить навигационные страницы. `--quiet`
отключает индикатор прогресса. Прогресс выводится только если `stderr` —
терминал; формат: `Загрузка: 12.3 MB / 100 MB (12%) 3.45 MB/s`, либо
`Загрузка: 12.3 MB 3.45 MB/s`, если `Content-Length` неизвестен.

### TUI

```bash
ipsuip --tui
```

Клавиши:

| Клавиша | Действие |
|---|---|
| `↑`/`↓`, `k`/`j` | выбор |
| `PgUp`/`PgDn`, `Home`/`End` | прокрутка |
| `Enter` | открыть страницу / сохранить IP-список |
| `Backspace`, `Esc`, `←` | назад |
| `s` (`F2`) | меню «Настройки» |
| `q` | выход |

Меню «Настройки» содержит пункты «Очистить кэш», «Прокси» и «Назад».
В диалоге «Прокси» строка проверяется перед применением; пустая строка
отключает прокси. Изменение применяется к последующим запросам сразу и
сохраняется в конфиг.

## Кэш и конфиг

| Объект | Linux | Windows |
|---|---|---|
| Кэш навигации | `$XDG_CACHE_HOME/ipsuip/data.json` (по умолчанию `~/.cache/ipsuip/data.json`) | `%LOCALAPPDATA%\ipsuip\data.json` |
| Конфиг | `$XDG_CONFIG_HOME/ipsuip/config.json` (по умолчанию `~/.config/ipsuip/config.json`) | `%APPDATA%\ipsuip\config.json` |

Пример `config.json`:

```json
{
  "proxy": "socks5://127.0.0.1:9050"
}
```

Поддерживаемые форматы прокси:

- `http://host:port`, `https://host:port` (по умолчанию порт 8080);
- `socks5://host:port`, `socks5h://host:port` (по умолчанию порт 1080);
- `host:port` — считается HTTP-прокси;
- `http://user:pass@host:port` — с базовой авторизацией (RFC 7617);
- `[::1]:1080` — IPv6-адрес;
- пустая строка — без прокси.

Приоритет: `--proxy` > конфиг > без прокси; `--no-proxy` принудительно
отключает прокси для данного запуска.

## Структура

```
CMakeLists.txt              OpenSSL (system shared), FetchContent, CPack
cmake/toolchains/           toolchain-файл для MinGW-w64
src/main.cpp                CLI и консольный режим
src/tui/                    TUI на PDCursesMod
src/http_client/            HTTPS-клиент на Boost.Asio/Beast
src/suip_model.cpp          разбор HTML и загрузка IP-списков
src/suip_cache.cpp          JSON-кэш навигации
src/config.cpp, paths.cpp   конфиг и платформенные пути
src/proxy.cpp               разбор и проверка прокси
src/progress.cpp            прогресс загрузки в stderr
PKGBUILD                    пакет для Arch Linux (AUR)
```
