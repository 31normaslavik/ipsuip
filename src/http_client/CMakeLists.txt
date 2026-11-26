# Синхронный HTTPS-клиент на Boost.Asio/Beast + OpenSSL.
# OpenSSL (ipsuip_ssl) настраивается в корневом CMakeLists.txt.

add_library(http_client_sync_ssl STATIC
    http_client_sync_ssl.cpp
    http_client_sync_ssl.h
    root_certificates.h
    ../proxy.cpp
    ../proxy.h
)

target_include_directories(http_client_sync_ssl
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src
           ${PROJECT_BINARY_DIR}/generated
)

target_link_libraries(http_client_sync_ssl
    PUBLIC Boost::headers Boost::system ipsuip_ssl
)
