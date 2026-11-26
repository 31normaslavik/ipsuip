# Contributing

## Быстрый старт

```bash
cmake --preset unix
cmake --build --preset unix
ctest --preset unix --output-on-failure
./build/unix/ipsuip --help
```

Для Windows используйте preset `windows` (Visual Studio 17 2022, x64).
Для самодостаточной сборки с OpenSSL в пакете — `bundle`.

Требования: CMake 3.28+, компилятор с C++20, Ninja, OpenSSL с заголовками
(для `bundle` вместо этого нужны Perl и Make).

## Правила

- Новый код — только через PR в `main`, CI должен быть зелёным.
- Логика разбора/преобразований покрывается тестами из `tests/`; тесты не
  должны обращаться к сети, используйте фикстуры из `tests/fixtures/`.
- Зависимости фиксируются версией **и** `URL_HASH` в корневом `CMakeLists.txt`;
  при обновлении зависимости обновляйте оба значения.
- Версия проекта задаётся только в файле `VERSION` (SemVer). Для PR,
  меняющих поведение, добавьте запись в раздел `Unreleased` CHANGELOG.md.
- Форматирование: `.clang-format`, статический анализ: `.clang-tidy`.

## Коммиты

Стиль Conventional Commits (`feat:`, `fix:`, `docs:`, `chore:`, `ci:`, `test:`).
Релиз создаётся по тегу `vX.Y.Z`; подробности — [docs/RELEASING.md](docs/RELEASING.md).
