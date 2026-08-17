find_package(wxWidgets CONFIG REQUIRED)

set(VORTCH_ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
configure_file(src/config.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/config.hpp @ONLY)

add_executable(vortch WIN32 src/main.cpp)

if(WIN32)
  target_sources(vortch PRIVATE src/platform_win.cpp)
elseif(APPLE)
  target_sources(vortch PRIVATE src/platform_mac.mm)
else()
  target_sources(vortch PRIVATE src/platform_linux.cpp)
endif()

target_include_directories(vortch PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(vortch PRIVATE vortch_core wx::core wx::base wx::adv)
