cmake_minimum_required(VERSION 3.20)

project(ipsuip LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -Wall -Wextra -Wpedantic
# -Wno-unused-parameter")

message("cmake_cxx_standard: ${CMAKE_CXX_STANDARD}")

set(SOURCES main.cpp functions.cpp timer.cpp parallel_parsing.cpp
            choose_your_fate.cpp http_client_sync_ssl.cpp)

message("Source files: ${SOURCES}")

set(HEADERS
    functions.h
    countries.h
    timer.h
    enum_variants.h
    parallel_parsing.h
    choose_your_fate.h
    http_client_sync_ssl.h)

message("Headers: ${HEADERS}")

set(Boost_USE_STATIC_LIBS ON) # only find static libs
set(Boost_USE_DEBUG_LIBS ON) # ignore debug libs and
set(Boost_USE_RELEASE_LIBS OFF) # only find release libs
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(
  Boost
  COMPONENTS # thread
  REQUIRED)

find_package(OpenSSL)
set(OPENSSL_USE_STATIC_LIBS ON)

# add_subdirectory(sync-ssl)

add_executable(ipsuip ${SOURCES} ${HEADERS})

target_link_libraries(ipsuip ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
message("Boost_libraries: ${Boost_LIBRARIES}")
message("Openssl_libraries: ${OPENSSL_LIBRARIES}")
