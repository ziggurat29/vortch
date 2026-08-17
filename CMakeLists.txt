cmake_minimum_required(VERSION 3.21)
project(vortch VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VORTCH_BUILD_UI     "Build the wxWidgets front end (vortch-ui)" OFF)
option(VORTCH_WITH_SQLITE  "Build the SQLite history store"            OFF)
option(VORTCH_BUILD_TESTS  "Build vortch-core unit tests"              ON)

if(VORTCH_BUILD_TESTS)
  enable_testing()
endif()

if(WIN32)
  add_compile_definitions(WINVER=0x0601 _WIN32_WINNT=0x0601)  # Windows 7 floor
endif()

add_subdirectory(vortch-core)

if(VORTCH_BUILD_UI)
  add_subdirectory(vortch-ui)
endif()
