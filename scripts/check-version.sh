#!/usr/bin/env bash
# Проверяет, что VERSION, PKGBUILD и CHANGELOG описывают одну версию.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="$(tr -d '[:space:]' < "${root}/VERSION")"

if [[ ! "${version}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "VERSION должен быть SemVer (X.Y.Z), получено: ${version}" >&2
  exit 1
fi

fail=0
if ! grep -qx "pkgver=${version}" "${root}/PKGBUILD"; then
  echo "PKGBUILD: pkgver должен быть ${version}" >&2
  fail=1
fi
if ! grep -qF "## [${version}]" "${root}/CHANGELOG.md"; then
  echo "CHANGELOG.md: нет секции [${version}]" >&2
  fail=1
fi
if ! grep -q "project(ipsuip VERSION \${IPSUIP_VERSION}" "${root}/CMakeLists.txt"; then
  echo "CMakeLists.txt: версия должна читаться из VERSION" >&2
  fail=1
fi

if (( fail )); then
  exit 1
fi
echo "Version metadata OK: ${version}"
