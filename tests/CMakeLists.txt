# Тесты не требуют сети: HTML-страницы хранятся в tests/fixtures.

add_executable(test_ip_convert test_ip_convert.cpp)
target_link_libraries(test_ip_convert PRIVATE ipsuip_tui)
add_test(NAME ip_convert COMMAND test_ip_convert)

add_executable(test_proxy test_proxy.cpp)
target_link_libraries(test_proxy PRIVATE http_client_sync_ssl)
add_test(NAME proxy COMMAND test_proxy)

add_executable(test_suip_model test_suip_model.cpp)
target_link_libraries(test_suip_model PRIVATE suip_core)
add_test(NAME suip_model COMMAND test_suip_model
  "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/continents.html"
  "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/ranges.html"
  "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/ranges.txt"
)

add_executable(test_config_cache test_config_cache.cpp)
target_link_libraries(test_config_cache PRIVATE suip_core)
add_test(NAME config_cache COMMAND test_config_cache)

add_test(NAME cli_help COMMAND ipsuip --help)
set_tests_properties(cli_help PROPERTIES
  PASS_REGULAR_EXPRESSION "ipsuip --tui")

add_test(NAME cli_version COMMAND ipsuip --version)
set_tests_properties(cli_version PROPERTIES
  PASS_REGULAR_EXPRESSION "ipsuip [0-9]+\\.[0-9]+\\.[0-9]+")

add_test(NAME cli_unknown_flag COMMAND ipsuip --definitely-unknown)
set_tests_properties(cli_unknown_flag PROPERTIES WILL_FAIL TRUE)
