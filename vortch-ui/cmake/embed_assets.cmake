# vortch_embed_assets(<out_cpp_var> <assets_root> <asset_file>...)
#
# Generates a C++ translation unit that embeds each asset file as a byte array,
# reachable by name via vortch::embedded_asset(). Uses only CMake built-ins
# (file(READ ... HEX)) so it needs no external tools and behaves identically on
# Windows/macOS/Linux. Names are the asset's path relative to <assets_root>
# (e.g. "icon.svg", "overlays/done.svg"). Bytes are stored verbatim, so this is
# format-agnostic: SVG today, raster later, no change here.
function(vortch_embed_assets OUT_CPP ASSETS_ROOT)
  set(_arrays "")
  set(_entries "")
  set(_idx 0)
  foreach(_f IN LISTS ARGN)
    file(RELATIVE_PATH _name "${ASSETS_ROOT}" "${_f}")
    file(READ "${_f}" _hex HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _bytes "${_hex}")
    string(APPEND _arrays "static const unsigned char asset_${_idx}[] = { ${_bytes} };\n")
    string(APPEND _entries "  { \"${_name}\", asset_${_idx}, sizeof(asset_${_idx}) },\n")
    # Re-run configure (regenerating the TU) when an asset's contents change.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_f}")
    math(EXPR _idx "${_idx}+1")
  endforeach()

  set(VORTCH_EMBED_ARRAYS "${_arrays}")
  set(VORTCH_EMBED_ENTRIES "${_entries}")
  configure_file(
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/embedded_assets.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.hpp" @ONLY)
  configure_file(
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/embedded_assets.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.cpp" @ONLY)
  set(${OUT_CPP} "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.cpp" PARENT_SCOPE)
endfunction()
