# Сборка, зависимости и пакеты

Версия для английского языка: [BUILD_EN.md](BUILD_EN.md)

## Зависимости

По умолчанию для сборки нужны компилятор C++20, CMake 3.28+, Ninja или Make
и OpenSSL 1.1.1+ с заголовками. `lexbor`, `Boost` и `PDCursesMod` CMake загрузит и
соберёт сам. Все загружаемые архивы зафиксированы по SHA256 в `CMakeLists.txt`.

По умолчанию OpenSSL используется как системная библиотека:

- для сборки нужны заголовки: в Arch — пакет `openssl`, в Debian/Ubuntu —
  `libssl-dev`;
- для запуска нужны `libssl`/`libcrypto`: в Arch — `openssl`, в Debian/Ubuntu —
  `libssl3` или `libssl3t64`, в Windows — DLL в `PATH` или рядом с
  `ipsuip.exe`;
- в стандартной сборке пакеты не включают OpenSSL: зависимость ставит
  менеджер пакетов.

Проверить после сборки: `ldd ./build/ipsuip | grep -E 'ssl|crypto'`.

### OpenSSL вместе с проектом

Для Windows, кросс-сборки или самодостаточного архива можно собрать
разделяемый OpenSSL из исходников:

```bash
cmake -S . -B build -DIPSUIP_BUNDLED_OPENSSL=ON
cmake --build build
```

Статический OpenSSL не используется. На Linux `libssl.so`/`libcrypto.so`
кладутся в `lib/` рядом с бинарником, на Windows DLL — в `bin/`; они попадают
в пакеты CPack. Такая сборка дольше и дополнительно требует Perl, Make,
а на Windows — NASM.

## Сборка и установка

### Linux

```bash
sudo pacman -S base-devel cmake ninja openssl
cmake --preset unix
cmake --build --preset unix
ctest --preset unix --output-on-failure
./build/unix/ipsuip --help
sudo cmake --install build/unix --prefix /usr/local
```

Пресеты (`unix`, `bundle`, `windows`) описаны в `CMakePresets.json`. Версию и
хэш коммита показывает `ipsuip --version`; единственный источник версии —
файл `VERSION`.

По умолчанию включена опция `IPSUIP_STATIC=ON`: зависимости проекта линкуются
статически, а системные libc и OpenSSL остаются динамическими. Если нужна
обычная динамическая сборка, используйте `-DIPSUIP_STATIC=OFF`.

### Windows

По умолчанию нужен OpenSSL SDK с заголовками и библиотеками; DLL должны лежать
рядом с `ipsuip.exe` или быть в `PATH`. Для самодостаточной сборки добавьте
`-DIPSUIP_BUNDLED_OPENSSL=ON` — тогда DLL попадут в пакет.

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

Для кросс-сборки со системным OpenSSL нужен OpenSSL для mingw-w64 (в Arch —
`mingw-w64-openssl` из AUR). Вариант с `-DIPSUIP_BUNDLED_OPENSSL=ON` собирает
OpenSSL вместе с проектом и работает без системного mingw-OpenSSL.

## Пакеты

```bash
cd build
cpack -G TGZ
cpack -G DEB
cpack -G ZIP
cpack -G NSIS
```

На Linux создаются TGZ и DEB, на macOS — TGZ, на Windows — ZIP и NSIS. В пакет входят
`ipsuip`, `README.md`, `README_EN.md`, `LICENSE`, `THIRD_PARTY_NOTICES.md`,
`CHANGELOG.md`, `CONTRIBUTING.md`, `SECURITY.md`, `docs/` и `LICENSES/`.
Со системным OpenSSL зависимости объявляются в DEB, с
`IPSUIP_BUNDLED_OPENSSL=ON` библиотеки кладутся прямо в пакет. Для Arch Linux
есть стабильный `PKGBUILD` (от тега): `makepkg -si`, и `ipsuip-git`
(`packaging/aur/ipsuip-git/PKGBUILD`) для ветки `main`.

Готовые архивы, контрольные суммы SHA256, SBOM и подписанный манифест
публикуются в [GitHub Releases](https://github.com/31normaslavik/ipsuip/releases)
по тегу `vX.Y.Z`. Порядок релиза описан в [RELEASING.md](RELEASING.md).

## Разработка

Тесты не требуют сети: фикстуры лежат в `tests/fixtures/`.

```bash
cmake --preset unix
cmake --build --preset unix
ctest --preset unix --output-on-failure
```

CI проверяет сборку на Linux (GCC/Clang), macOS, Windows (MSVC), кросс-сборку
MinGW-w64 и запуск тестов под ASan/UBSan. См.
[CONTRIBUTING.md](../CONTRIBUTING.md) и [SECURITY.md](../SECURITY.md).
