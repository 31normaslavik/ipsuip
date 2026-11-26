# ipsuip

[![CI](https://github.com/31normaslavik/ipsuip/actions/workflows/ci.yml/badge.svg)](https://github.com/31normaslavik/ipsuip/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/31normaslavik/ipsuip)](https://github.com/31normaslavik/ipsuip/releases)

Russian version: [README.md](README.md)

`ipsuip` is a utility for parsing IP addresses from [suip.biz](https://suip.biz/).
It helps you find address ranges for a specific country or city and save them
in Range or CIDR format.

It works both in the command line and in an interactive menu.

## Features

- navigation: continent → country → region → city;
- fetch IP ranges and save as Range/CIDR;
- CLI and TUI;
- HTTP/SOCKS5 proxy configuration;
- download progress indicator;
- navigation cache and cache clearing.

## Dependencies

By default, building requires a C++20 compiler, CMake 3.28+, Ninja or Make,
and OpenSSL 1.1.1+ with headers. CMake will download and build `lexbor`, `Boost`,
and `PDCursesMod` automatically. Every downloaded archive is pinned by SHA256
in `CMakeLists.txt`.

By default, OpenSSL is used as a system library:

- headers are required for building: on Arch — the `openssl` package, on
  Debian/Ubuntu — `libssl-dev`;
- `libssl`/`libcrypto` are required at runtime: on Arch — `openssl`, on
  Debian/Ubuntu — `libssl3` or `libssl3t64`, on Windows — DLLs in `PATH` or next
  to `ipsuip.exe`;
- standard packages do not bundle OpenSSL: the dependency is installed by the
  package manager.

Check after building: `ldd ./build/ipsuip | grep -E 'ssl|crypto'`.

### Bundled OpenSSL

For Windows, cross-compilation, or a self-contained archive, you can build a
shared OpenSSL from source:

```bash
cmake -S . -B build -DIPSUIP_BUNDLED_OPENSSL=ON
cmake --build build
```

Static OpenSSL is not used. On Linux, `libssl.so`/`libcrypto.so` are placed in
`lib/` next to the binary; on Windows, DLLs are placed in `bin/`. They are
included in CPack packages. This build takes longer and additionally requires
Perl, Make, and on Windows — NASM.

## Build and installation

### Linux

```bash
sudo pacman -S base-devel cmake ninja openssl
cmake --preset unix
cmake --build --preset unix
ctest --preset unix --output-on-failure
./build/unix/ipsuip --help
sudo cmake --install build/unix --prefix /usr/local
```

Presets (`unix`, `bundle`, `windows`) are defined in `CMakePresets.json`.
`ipsuip --version` prints the version and commit hash; the single source of
truth for the version is the `VERSION` file.

By default, `IPSUIP_STATIC=ON` is enabled: project dependencies are linked
statically, while system libc and OpenSSL remain dynamic. If you need a regular
dynamic build, use `-DIPSUIP_STATIC=OFF`.

### Windows

By default, an OpenSSL SDK with headers and libraries is required; DLLs must be
next to `ipsuip.exe` or in `PATH`. For a self-contained build, add
`-DIPSUIP_BUNDLED_OPENSSL=ON` — then the DLLs will be included in the package.

MSVC:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

MinGW-w64 (including cross-compilation from Linux):

```bash
cmake -S . -B build-mingw -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw
```

For cross-compilation with system OpenSSL, you need OpenSSL for mingw-w64 (on
Arch — `mingw-w64-openssl` from AUR). The `-DIPSUIP_BUNDLED_OPENSSL=ON` option
builds OpenSSL together with the project and works without a system
mingw-OpenSSL.

## Packages

```bash
cd build
cpack -G TGZ
cpack -G DEB
cpack -G ZIP
cpack -G NSIS
```

On Linux, TGZ and DEB packages are created; on Windows — ZIP and NSIS. Packages
include `ipsuip`, `README.md`, `LICENSE`, `THIRD_PARTY_NOTICES.md`, and
`LICENSES/`. With system OpenSSL, dependencies are declared in DEB; with
`IPSUIP_BUNDLED_OPENSSL=ON`, libraries are placed directly into the package.
For Arch Linux there is a stable `PKGBUILD` (tag-based): `makepkg -si`, and
`ipsuip-git` (`packaging/aur/ipsuip-git/PKGBUILD`) tracking `main`.

Ready-made archives, SHA256 checksums, an SBOM, and a signed manifest are
published in [GitHub Releases](https://github.com/31normaslavik/ipsuip/releases)
for each `vX.Y.Z` tag. See [docs/RELEASING.md](docs/RELEASING.md).

## Usage

### CLI

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

### TUI

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

## Development

Tests are offline and use fixtures from `tests/fixtures/`.

```bash
cmake --preset unix
cmake --build --preset unix
ctest --preset unix --output-on-failure
```

CI builds on Linux (GCC/Clang), macOS, Windows (MSVC), cross-compiles with
MinGW-w64, and runs the tests under ASan/UBSan. See
[CONTRIBUTING.md](CONTRIBUTING.md) and [SECURITY.md](SECURITY.md).

## License

`ipsuip` is distributed under the [MIT License](LICENSE). Third-party licenses
and attribution are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md);
full texts are in [`LICENSES/`](LICENSES/).
