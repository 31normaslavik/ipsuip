# Build, dependencies and packages

Русская версия: [BUILD.md](BUILD.md)

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

On Linux, TGZ and DEB packages are created; on macOS — TGZ; on Windows — ZIP
and NSIS. Packages include `ipsuip`, `README.md`, `README_EN.md`, `LICENSE`,
`THIRD_PARTY_NOTICES.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, `SECURITY.md`,
`docs/`, and `LICENSES/`. With system OpenSSL, dependencies are
declared in DEB; with `IPSUIP_BUNDLED_OPENSSL=ON`, libraries are placed directly
into the package. For Arch Linux there is a stable `PKGBUILD` (tag-based):
`makepkg -si`, and `ipsuip-git` (`packaging/aur/ipsuip-git/PKGBUILD`) tracking
the `main` branch.

Ready-made archives, SHA256 checksums, an SBOM, and a signed manifest are
published in [GitHub Releases](https://github.com/31normaslavik/ipsuip/releases)
for each `vX.Y.Z` tag. See [RELEASING.md](RELEASING.md) for the release process.

## Development

Tests are offline and use fixtures from `tests/fixtures/`.

```bash
cmake --preset unix
cmake --build --preset unix
ctest --preset unix --output-on-failure
```

CI builds on Linux (GCC/Clang), macOS, Windows (MSVC), cross-compiles with
MinGW-w64, and runs the tests under ASan/UBSan. See
[CONTRIBUTING.md](../CONTRIBUTING.md) and [SECURITY.md](../SECURITY.md).
