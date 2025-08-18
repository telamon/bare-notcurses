cmake_minimum_required(VERSION 3.27)

find_package(cmake-fetch REQUIRED PATHS node_modules/cmake-fetch)
find_package(cmake-bare REQUIRED PATHS node_modules/cmake-bare)
find_package(cmake-napi REQUIRED PATHS node_modules/cmake-napi)
find_package(cmake-npm REQUIRED PATHS node_modules/cmake-npm)

project(notcurses_native CXX C)

bare_target(target)

# add_compile_options(-fPIC) # sledge hammer
# add_link_options(-static) # no workie for addons
# set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

set(NOTCURSES_DIR "" CACHE PATH "Leave empty for auto-fetch")

set(BUILD_SHARED_LIBS OFF)

set(libnotcurses notcurses-core)

# configure notcurses
set(USE_MULTIMEDIA "none")
set(USE_CXX OFF)
set(USE_STATIC ON)
set(USE_DOCTEST OFF)
set(BUILD_EXECUTABLES OFF)
set(BUILD_FFI_LIBRARY OFF)
set(USE_POC OFF)

if ("${NOTCURSES_DIR}" STREQUAL "")
  fetch_package("github:dankamongmen/notcurses#e6aeee5" SOURCE_DIR NOTCURSES_DIR)
else()
  add_subdirectory(${NOTCURSES_DIR} ${CMAKE_BINARY_DIR}/notcurses)
endif()
message("using notcurses: ${NOTCURSES_DIR}")

# fetch_package("github:ThomasDickey/ncurses-snapshots")
# fetch_package("github:mity/md4c#729e6b8" SOURCE_DIR md4c_src)
fetch_package("github:holepunchto/libjstl#122cbdd")

add_bare_module(notcurses_bare)
target_sources(
  ${notcurses_bare}
  PRIVATE
    binding.cc
)

target_link_libraries(
  ${notcurses_bare}
  PRIVATE
    $<TARGET_OBJECTS:${libnotcurses}>
    jstl
    # TODO static link
    ncurses
    unistring
)

target_include_directories(
  ${notcurses_bare}
  PRIVATE
    "${NOTCURSES_DIR}/include/"
)

if (OFF)
add_napi_module(notcurses_napi)
target_sources(
  ${notcurses_napi}
  PRIVATE
    binding.cc
)

target_link_libraries(
  ${notcurses_napi}
  PRIVATE
    $<TARGET_OBJECTS:${libnotcurses}>
    jstl
    # TODO static link
    ncurses
    unistring
)

resolve_node_module(bare-compat-napi compat)

target_include_directories(
  ${notcurses_napi}
  PRIVATE
    "${compat}/include" # TODO fails
    "${NOTCURSES_DIR}/include/"
)
endif()
