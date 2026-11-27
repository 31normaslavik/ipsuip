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

## Quick start

Ready-made archives and packages are published in
[GitHub Releases](https://github.com/31normaslavik/ipsuip/releases).
For Arch Linux, the AUR packages `ipsuip` (from a tag) and `ipsuip-git` (the
`main` branch) are available.

Build from source (Linux, CMake 3.28+, C++20, Ninja, OpenSSL with headers):

```bash
cmake --preset unix
cmake --build --preset unix
./build/unix/ipsuip --help
```

Usage examples:

```bash
ipsuip --mainland
ipsuip --mainland EU --country RU --province Chelyabinsk --city Kartaly --output ./out
ipsuip --tui
```

## Documentation

- [Build, dependencies and packages](docs/BUILD_EN.md)
- [Usage: CLI, TUI, cache and config](docs/USAGE_EN.md)
- [Contributing](CONTRIBUTING.md)
- [Release checklist](docs/RELEASING.md)
- [Reporting a vulnerability](SECURITY.md)
- [Changelog](CHANGELOG.md)

## License

`ipsuip` is distributed under the [MIT License](LICENSE). Third-party licenses
and attribution are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md);
full texts are in [`LICENSES/`](LICENSES/).
