# Релизный чек-лист

1. Убедиться, что `main` зелёный в CI и CodeQL.
2. Переместить записи из `Unreleased` в новую секцию `CHANGELOG.md` с датой.
3. Обновить `VERSION` (SemVer: `MAJOR.MINOR.PATCH`) и `pkgver` в `PKGBUILD`
   (`scripts/check-version.sh` должен проходить). При смене версии зависимостей
   обновить версию **и** `URL_HASH` в `CMakeLists.txt`.
4. Проверить локально:

   ```bash
   cmake --preset unix
   cmake --build --preset unix
   ctest --preset unix --output-on-failure
   ./build/unix/ipsuip --version
   ```

5. Закоммитить изменения и запушить в `main`.
6. Создать аннотированный тег (желательно подписанный):

   ```bash
   git tag -s v"$(cat VERSION)" -m "ipsuip v$(cat VERSION)"
   git push origin v"$(cat VERSION)"
   ```

   Если подпись не настроена: `git tag -a ...` (без `-s`).

7. Workflow `.github/workflows/release.yml` соберёт пакеты для Linux, Windows,
   macOS и MinGW, посчитает `SHA256SUMS`, создаст SBOM и подпишет манифест,
   затем опубликует GitHub Release с авто-notes.
8. Проверить релиз: наличие всех архивов, `SHA256SUMS`, `SHA256SUMS.bundle`,
   `ipsuip-sbom.spdx.json`, корректность `--version` в бинарнике.
9. Обновить AUR: для `ipsuip` заменить `SKIP` на реальную контрольную сумму архива тега
   (`bash scripts/aur-checksum.sh`) и переключить `source` на tarball;
   `ipsuip-git` остаётся на ветке `main`.

## Откат

Удалить тег (`git tag -d vX.Y.Z && git push origin :refs/tags/vX.Y.Z`) и
отменить публикацию релиза в интерфейсе GitHub. Не переиспользовать
опубликованные версии: следующая сборка должна получить новый `PATCH`.
