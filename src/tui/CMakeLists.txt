# Модуль интерактивного TUI на PDCursesMod (порт vt на Unix, wincon на Windows):
# просмотр иерархии suip.biz, настройки и сохранение IP-списков.

add_library(ipsuip_tui STATIC
    ipsuip_tui.cpp ipsuip_tui.h
    ip_convert.cpp ip_convert.h
)

target_include_directories(ipsuip_tui
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(ipsuip_tui
    PUBLIC suip_core ipsuip_curses
)
