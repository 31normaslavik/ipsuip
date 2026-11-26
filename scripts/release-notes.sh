#!/usr/bin/env bash
# Печатает заметки релиза из секции VERSION CHANGELOG.md.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="$(tr -d '[:space:]' < "${root}/VERSION")"

printf '# v%s\n\n' "${version}"
awk -v ver="${version}" '
  !found && index($0, "## [" ver "]") == 1 { found = 1; next }
  found && (/^## \[/ || /^\[.*\]:/) { exit }
  found && n == 0 && $0 == "" { next }
  found { lines[++n] = $0 }
  END {
    while (n > 0 && lines[n] == "") n--
    for (i = 1; i <= n; i++) print lines[i]
  }
' "${root}/CHANGELOG.md"
