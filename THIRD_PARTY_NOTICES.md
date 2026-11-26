# Сторонние компоненты / Third-party notices

Код `ipsuip` распространяется под [MIT](LICENSE). Ниже перечислены лицензии
зависимостей, которые используются при сборке и/или попадают в готовые пакеты.
Полные тексты лицензий находятся в каталоге [`LICENSES/`](LICENSES/).

| Компонент | Версия | Лицензия (SPDX) | Примечание |
|---|---|---|---|
| [Boost](https://www.boost.org/) | 1.92.0 | `BSL-1.0` | Загружается CMake: `asio`, `beast`, `json`, `system` и заголовочные зависимости. |
| [lexbor](https://github.com/lexbor/lexbor) | 3.0.1 | `Apache-2.0`, частично `BSD-2-Clause` | Статически линкуется. Часть файлов `core/dtoa.*`, `core/strtod.*`, `core/diyfp.*` основана на NGINX NJS (`BSD-2-Clause`). |
| [PDCursesMod](https://github.com/Bill-Gray/PDCursesMod) | 4.5.4 | Public domain (ядро), отдельные файлы — под другими лицензиями | Используются `pdcurses/*.c` и порт `vt` (Linux) / `wincon` (Windows). Атрибуция не обязательна, но сохранена. |
| [OpenSSL](https://www.openssl.org/) | 3.6.5 | `Apache-2.0` with OpenSSL exception | По умолчанию системная shared-библиотека. При `IPSUIP_BUNDLED_OPENSSL=ON` библиотеки и их лицензия включаются в пакет. |

## Происхождение `src/http_client/root_certificates.h`

Файл с набором корневых сертификатов основан на примере
[Boost.Beast `root_certificates.hpp`](https://github.com/boostorg/beast/blob/develop/example/common/root_certificates.hpp):

```text
Copyright (c) 2016-2019 Vinnie Falco
SPDX-License-Identifier: BSL-1.0
```

Сами сертификаты выданы соответствующими удостоверяющими центрами и не являются
объектом авторского права `ipsuip`.

## Статическая линковка

`IPSUIP_STATIC=ON` линкует Boost, lexbor и PDCursesMod статически. Поэтому
тексты их лицензий должны сопровождать распространяемые бинарные пакеты — они
устанавливаются вместе с `LICENSE` и этим файлом в `share/doc/ipsuip/`.
