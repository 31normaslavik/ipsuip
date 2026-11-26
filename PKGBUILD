# Maintainer: 31normaslavik <16459524+31normaslavik@users.noreply.github.com>
# Пакет для Arch Linux (AUR).
# git-источник: sha256sums=('SKIP') — нормальная практика для AUR -git пакетов.
pkgname=ipsuip
pkgver=0.1.0
pkgrel=1
pkgdesc='CLI и TUI для скачивания IP-диапазонов suip.biz с экспортом Range/CIDR'
arch=('x86_64' 'aarch64')
url='https://github.com/31normaslavik/ipsuip_develop'
license=('MIT')
# OpenSSL подключается разделяемо: нужен и при сборке, и в рантайме.
depends=('glibc' 'gcc-libs' 'openssl')
makedepends=('cmake' 'ninja' 'git')
source=("${pkgname}::git+${url}#branch=main")
sha256sums=('SKIP')

build() {
  local src="${srcdir}/${pkgname}"
  # lexbor, Boost и PDCursesMod загружаются CMake; OpenSSL — системный (shared).
  cmake -S "${src}" -B "${srcdir}/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DIPSUIP_STATIC=ON \
    -DIPSUIP_BUNDLED_OPENSSL=OFF
  cmake --build "${srcdir}/build"
}

check() {
  # Тестов в проекте пока нет.
  "${srcdir}/build/ipsuip" --help > /dev/null
}

package() {
  DESTDIR="${pkgdir}" cmake --install "${srcdir}/build"
  install -Dm644 "${srcdir}/${pkgname}/README.md" \
    "${pkgdir}/usr/share/doc/${pkgname}/README.md"
  install -Dm644 "${srcdir}/${pkgname}/LICENSE" \
    "${pkgdir}/usr/share/doc/${pkgname}/LICENSE"
  install -Dm644 "${srcdir}/${pkgname}/THIRD_PARTY_NOTICES.md" \
    "${pkgdir}/usr/share/doc/${pkgname}/THIRD_PARTY_NOTICES.md"
  cp -r "${srcdir}/${pkgname}/LICENSES" \
    "${pkgdir}/usr/share/doc/${pkgname}/LICENSES"
}
