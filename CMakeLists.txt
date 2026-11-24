cmake_minimum_required(VERSION 3.30)

project(ipsuip_develop VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(lexbor REQUIRED)
# target_link_libraries(your_target PRIVATE liblexbor)

add_subdirectory(http_client)

# Общая модель suip.biz и JSON-кэш навигационных страниц.
# Используется консольным режимом и TUI.
add_library(suip_core STATIC
    suip_model.cpp suip_model.h
    suip_cache.cpp suip_cache.h
)
target_include_directories(suip_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(suip_core PUBLIC lexbor http_client_sync_ssl)

add_subdirectory(tui)

add_executable(ipsuip_develop main.cpp)
target_link_libraries(ipsuip_develop PRIVATE suip_core ipsuip_tui -lpthread -lm -static-libgcc -static-libstdc++)

# Install the target
install(
    TARGETS ipsuip_develop
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# target_link_libraries(ipsuip PRIVATE ${Boost_LIBRARIES} )
message("Boost_libraries: ${Boost_LIBRARIES}")
message("Openssl_libraries: ${OPENSSL_LIBRARIES}")
