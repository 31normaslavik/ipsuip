#!/usr/bin/env bash
# Печатает sha256 архива тега для стабильного PKGBUILD из AUR.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="$(tr -d '[:space:]' < "${root}/VERSION")"
url="https://github.com/31normaslavik/ipsuip/archive/refs/tags/v${version}.tar.gz"

tmp="$(mktemp)"
trap 'rm -f "${tmp}"' EXIT
if ! curl -fsSL "${url}" -o "${tmp}"; then
  echo "Не удалось скачать архив тега: ${url}" >&2
  echo "Проверьте, что тег опубликован и репозиторий доступен без авторизации." >&2
  exit 1
fi

sum="$(sha256sum "${tmp}" | cut -d' ' -f1)"
echo "Подставьте в PKGBUILD:"
echo "  source=(\"\${pkgname}::${url}\")"
echo "  sha256sums=('${sum}')"
