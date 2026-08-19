find_package(wxWidgets CONFIG REQUIRED)

# Embed assets (icons, etc.) as byte arrays compiled into the binary, so the
# executable is self-contained and relocatable — no external asset files, no
# baked absolute path. See cmake/embed_assets.cmake.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_assets.cmake)
file(GLOB_RECURSE VORTCH_ASSET_FILES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/assets/*.svg")
vortch_embed_assets(VORTCH_EMBEDDED_CPP "${CMAKE_SOURCE_DIR}/assets"
  ${VORTCH_ASSET_FILES})

add_executable(vortch WIN32 src/main.cpp ${VORTCH_EMBEDDED_CPP})

if(WIN32)
  target_sources(vortch PRIVATE src/platform_win.cpp)
elseif(APPLE)
  target_sources(vortch PRIVATE src/platform_mac.mm)
else()
  target_sources(vortch PRIVATE src/platform_linux.cpp)
  # platform_linux.cpp uses GTK directly (window type hint to suppress the WM
  # drop-shadow on the shaped gadget). GTK is already present as wxGTK's backend.
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
  target_link_libraries(vortch PRIVATE PkgConfig::GTK3)
endif()

target_include_directories(vortch PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(vortch PRIVATE vortch_core wx::core wx::base wx::adv)
