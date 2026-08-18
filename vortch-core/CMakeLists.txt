find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

add_library(vortch_core STATIC
  src/store.cpp)
target_include_directories(vortch_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(vortch_core PUBLIC cxx_std_17)
target_link_libraries(vortch_core PUBLIC
  nlohmann_json::nlohmann_json
  unofficial::sqlite3::sqlite3)

if(VORTCH_BUILD_TESTS)
  find_package(doctest CONFIG REQUIRED)
  add_executable(vortch_core_tests
    tests/test_main.cpp
    tests/test_resource_ref.cpp
    tests/test_aggregator.cpp
    tests/test_status_machine.cpp
    tests/test_config_document.cpp
    tests/test_identity.cpp
    tests/test_launch.cpp
    tests/test_text.cpp
    tests/test_store.cpp)
  target_link_libraries(vortch_core_tests PRIVATE vortch_core doctest::doctest)
  add_test(NAME vortch_core_tests COMMAND vortch_core_tests)
endif()
