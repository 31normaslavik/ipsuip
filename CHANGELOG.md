# Changelog

Все значимые изменения фиксируются здесь. Формат основан на
[Keep a Changelog](https://keepachangelog.com/ru/1.1.0/), версии следуют
[Semantic Versioning](https://semver.org/lang/ru/).

## [Unreleased]

### Added

- GitHub Actions: CI (Linux/macOS/Windows, MinGW, ASan/UBSan), CodeQL,
  Dependabot и автоматический релиз по тегу `v*`.
- Юнит-тесты и CTest для CIDR, прокси, разбора страниц, конфига и кэша.
- `--version` и единый источник версии в файле `VERSION`.
- Контрольные суммы SHA256, SPDX-SBOM и подпись манифеста в релизах.

### Changed

- Корневые сертификаты системы используются на Unix, встроенный набор —
  как фолбэк и на Windows.
- `config.json` создаётся с правами `0600` на Unix.

### Removed

- Неиспользуемая отладочная функция `lexbor_test()`.

## [0.1.0] - 2026-11-26

### Added

- CLI и TUI для навигации по иерархии suip.biz.
- Экспорт IP-диапазонов в Range и CIDR, кэш навигации, поддержка прокси.
- Сборка CMake/CPack (TGZ, DEB, ZIP, NSIS) и PKGBUILD для Arch Linux.

[Unreleased]: https://github.com/31normaslavik/ipsuip/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/31normaslavik/ipsuip/releases/tag/v0.1.0
