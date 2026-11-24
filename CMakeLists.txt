cmake_minimum_required(VERSION 3.28)

# lexbor объявляет совместимость с CMake 2.8; CMake 4.x требует минимум 3.5.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
  set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE STRING "" FORCE)
endif()

project(ipsuip VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(FetchContent)
include(CheckSymbolExists)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Опции сборки
# ---------------------------------------------------------------------------

option(IPSUIP_STATIC "Статически линковать рантайм GCC и зависимости проекта" ON)

set(IPSUIP_BOOST_VERSION "1.92.0" CACHE STRING "Версия Boost (модульный CMake-архив)")
set(IPSUIP_LEXBOR_VERSION "3.0.1" CACHE STRING "Версия lexbor")
set(IPSUIP_PDCURSES_VERSION "4.5.4" CACHE STRING "Версия PDCursesMod")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Тип сборки" FORCE)
endif()

# Asio не должен подтягивать Boost.DateTime: он не входит в замыкание заголовков.
add_compile_definitions(BOOST_ASIO_DISABLE_BOOST_DATE_TIME BOOST_ALL_NO_LIB)

if(MSVC)
  # /MT для Release и /MTd для Debug.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_compile_options(/MP)
elseif(IPSUIP_STATIC)
  # Только рантайм GCC и зависимости проекта статические. libc и OpenSSL
  # остаются динамическими: OpenSSL НЕ линкуется статически (см. ниже).
  add_link_options(-static-libgcc -static-libstdc++)
endif()

if(WIN32)
  set(IPSUIP_PLATFORM_LIBS ws2_32 wsock32 crypt32 bcrypt)
else()
  set(IPSUIP_PLATFORM_LIBS ${CMAKE_DL_LIBS})
endif()

# ---------------------------------------------------------------------------
# OpenSSL: только системная разделяемая библиотека (НЕ статическая).
# libssl.so / libcrypto.so нужны и при сборке (заголовки + .so), и в рантайме.
# Проверяем сразу, до загрузки FetchContent-зависимостей, чтобы отсутствие
# OpenSSL приводило к понятной ошибке конфигурации.
# ---------------------------------------------------------------------------

set(OPENSSL_USE_STATIC_LIBS FALSE)
set(OPENSSL_MSVC_STATIC_RT FALSE)
find_package(OpenSSL 1.1.1 REQUIRED COMPONENTS SSL Crypto)
message(STATUS "OpenSSL: ${OPENSSL_VERSION} (shared; required at runtime)")

add_library(ipsuip_ssl INTERFACE)
target_link_libraries(ipsuip_ssl INTERFACE
  OpenSSL::SSL OpenSSL::Crypto ${IPSUIP_PLATFORM_LIBS}
)

# ---------------------------------------------------------------------------
# lexbor (FetchContent, только статическая библиотека)
# ---------------------------------------------------------------------------

set(LEXBOR_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_TESTS_CPP OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_UTILS OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(LEXBOR_INSTALL_HEADERS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(lexbor
  URL "https://github.com/lexbor/lexbor/archive/refs/tags/v${IPSUIP_LEXBOR_VERSION}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lexbor)

# ---------------------------------------------------------------------------
# Boost (FetchContent модулей): только то, что реально включается кодом.
# Остальные зависимости BoostRoot определит сам из CMakeLists выбранных модулей.
# ---------------------------------------------------------------------------

set(BOOST_INCLUDE_LIBRARIES
  asio beast json system
  config container intrusive optional smart_ptr mp11 endian winapi
  # include-замыкание Asio/Beast/JSON/System и перечисленных выше модулей
  align assert core detail integer io move predef preprocessor
  static_assert throw_exception type_traits utility variant2
  function bind array ratio range
)
set(BOOST_ENABLE_COMPATIBILITY_TARGETS ON)
set(BOOST_SKIP_INSTALL_RULES ON)
set(BOOST_RUNTIME_LINK static)
set(BOOST_ENABLE_MPI OFF)
set(BOOST_ENABLE_PYTHON OFF)
set(BUILD_TESTING OFF)

FetchContent_Declare(boost
  URL "https://github.com/boostorg/boost/releases/download/boost-${IPSUIP_BOOST_VERSION}/boost-${IPSUIP_BOOST_VERSION}-cmake.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(boost)

# ---------------------------------------------------------------------------
# PDCursesMod (FetchContent): только порт wincon (Windows) или vt (Unix).
# Используем Populate без add_subdirectory upstream-проекта: его CMake
# собирает демо-цели и лишние порты, а нам нужна одна статическая библиотека.
# ---------------------------------------------------------------------------

if(WIN32)
  set(PDCURSES_PORT wincon)
else()
  set(PDCURSES_PORT vt)
endif()

FetchContent_Declare(pdcurses
  URL "https://github.com/Bill-Gray/PDCursesMod/archive/refs/tags/v${IPSUIP_PDCURSES_VERSION}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(pdcurses)
if(NOT pdcurses_POPULATED)
  # Нам нужны только исходники: upstream-CMake собирает демо и лишние порты.
  if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
  endif()
  FetchContent_Populate(pdcurses)
  if(POLICY CMP0169)
    cmake_policy(SET CMP0169 NEW)
  endif()
endif()

file(GLOB PDCURSES_COMMON_SOURCES CONFIGURE_DEPENDS
  "${pdcurses_SOURCE_DIR}/pdcurses/*.c")
set(PDCURSES_PORT_SOURCES "")
foreach(pdc_file pdcclip.c pdcdisp.c pdcgetsc.c pdckbd.c pdcscrn.c pdcsetsc.c pdcutil.c)
  list(APPEND PDCURSES_PORT_SOURCES "${pdcurses_SOURCE_DIR}/${PDCURSES_PORT}/${pdc_file}")
endforeach()

add_library(ipsuip_curses STATIC
  ${PDCURSES_COMMON_SOURCES} ${PDCURSES_PORT_SOURCES}
)
target_include_directories(ipsuip_curses
  PUBLIC "${pdcurses_SOURCE_DIR}"
  PRIVATE "${pdcurses_SOURCE_DIR}/${PDCURSES_PORT}"
)
target_compile_definitions(ipsuip_curses PUBLIC PDC_FORCE_UTF8)
check_symbol_exists(vsnprintf "stdio.h" HAVE_VSNPRINTF)
check_symbol_exists(vsscanf "stdio.h" HAVE_VSSCANF)
if(HAVE_VSNPRINTF)
  target_compile_definitions(ipsuip_curses PRIVATE HAVE_VSNPRINTF)
endif()
if(HAVE_VSSCANF)
  target_compile_definitions(ipsuip_curses PRIVATE HAVE_VSSCANF)
endif()
if(WIN32)
  target_compile_definitions(ipsuip_curses PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_link_libraries(ipsuip_curses PUBLIC
    gdi32 winspool shell32 ole32 comdlg32 advapi32 winmm user32
  )
endif()

# ---------------------------------------------------------------------------
# Проект
# ---------------------------------------------------------------------------

add_subdirectory(src/http_client)

# Общая модель suip.biz, кэш, конфиг, пути и индикатор прогресса.
add_library(suip_core STATIC
  src/suip_model.cpp src/suip_model.h
  src/suip_cache.cpp src/suip_cache.h
  src/config.cpp     src/config.h
  src/paths.cpp      src/paths.h
  src/progress.cpp   src/progress.h
)
target_include_directories(suip_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(suip_core PUBLIC
  lexbor_static http_client_sync_ssl Boost::json
)

add_subdirectory(src/tui)

add_executable(ipsuip src/main.cpp)
target_link_libraries(ipsuip PRIVATE
  suip_core ipsuip_tui ipsuip_curses
  Threads::Threads
)

install(TARGETS ipsuip
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  BUNDLE DESTINATION .
)
install(FILES README.md DESTINATION "${CMAKE_INSTALL_DOCDIR}")

# ---------------------------------------------------------------------------
# CPack: ZIP/NSIS для Windows, TGZ/DEB для Linux
# ---------------------------------------------------------------------------

set(CPACK_PACKAGE_NAME "ipsuip")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "ipsuip")
set(CPACK_PACKAGE_CONTACT "https://github.com/31normaslavik/ipsuip_develop")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/31normaslavik/ipsuip_develop")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Просмотр и экспорт IP-списков suip.biz")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "ipsuip")

if(WIN32)
  set(CPACK_GENERATOR "ZIP;NSIS")
  set(CPACK_NSIS_DISPLAY_NAME "ipsuip")
  set(CPACK_NSIS_PACKAGE_NAME "ipsuip")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
else()
  set(CPACK_GENERATOR "TGZ;DEB")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "ipsuip maintainers")
  set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
  set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
  # OpenSSL линкуется динамически — пакету нужна системная libssl.
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.28), libssl3 | libssl3t64")
endif()

include(CPack)
