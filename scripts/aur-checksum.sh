#!/usr/bin/env bash
# Печатает sha256 архива тега для стабильного PKGBUILD из AUR.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="$(tr -d '[:space:]' < "${root}/VERSION")"
url="https://github.com/31normaslavik/ipsuip/archive/refs/tags/v${version}.tar.gz"

echo "Проверьте, что тег v${version} опубликован, затем подставьте:"
echo "  source=(\"\${pkgname}::${url}\")"
echo "  sha256sums=('$(curl -fsSL "${url}" | sha256sum | cut -d' ' -f1)')"
