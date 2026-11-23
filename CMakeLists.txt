cmake_minimum_required(VERSION 3.30)

project(ipsuip_develop VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES main.cpp)
# set(SOURCES fle.cpp)
# set(SOURCES fle2.cpp)
# set(SOURCES fle3.cpp)

find_package(lexbor REQUIRED)
# target_link_libraries(your_target PRIVATE liblexbor)

add_subdirectory(http_client)

add_executable(ipsuip_develop ${SOURCES} ${HEADERS})
target_link_libraries(ipsuip_develop PRIVATE lexbor http_client_sync_ssl -lpthread -lm -static-libgcc -static-libstdc++)

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
