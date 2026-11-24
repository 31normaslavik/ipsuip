# Модуль интерактивного TUI (ncursesw): просмотр иерархии suip.biz
# и сохранение IP-списков городов.

find_package(PkgConfig REQUIRED)
pkg_check_modules(NCURSESW REQUIRED IMPORTED_TARGET ncursesw)

add_library(ipsuip_tui STATIC
    ipsuip_tui.cpp ipsuip_tui.h
    ip_convert.cpp ip_convert.h
)

# Каталог модуля нужен для собственных заголовков, корень проекта — для suip_model.h.
target_include_directories(ipsuip_tui
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
           ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ipsuip_tui
    PUBLIC suip_core PkgConfig::NCURSESW
)
