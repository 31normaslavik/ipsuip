# Maintainer: 31normaslavik <16459524+31normaslavik@users.noreply.github.com>
# Стабильный пакет для Arch Linux (AUR): источник привязан к тегу v${pkgver}.
# Для слежения за main используйте packaging/aur/ipsuip-git/PKGBUILD.
# sha256sums=('SKIP') допустим только для git-источника; после публикации
# архива тега переключите source на tarball и подставьте сумму из
# scripts/aur-checksum.sh.
pkgname=ipsuip
pkgver=0.1.0
pkgrel=1
pkgdesc='CLI и TUI для скачивания IP-диапазонов suip.biz с экспортом Range/CIDR'
arch=('x86_64' 'aarch64')
url='https://github.com/31normaslavik/ipsuip'
license=('MIT')
# OpenSSL подключается разделяемо: нужен и при сборке, и в рантайме.
depends=('glibc' 'gcc-libs' 'openssl')
makedepends=('cmake' 'ninja' 'git')
source=("${pkgname}::git+${url}#tag=v${pkgver}")
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
  "${srcdir}/build/ipsuip" --help > /dev/null
  "${srcdir}/build/ipsuip" --version | grep -q "^ipsuip ${pkgver}"
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
