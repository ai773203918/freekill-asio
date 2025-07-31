# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.22)

project(freekill-asio VERSION 0.0.1)

if (DEFINED RPC_DEBUG)
  add_definitions(-DRPC_DEBUG)
endif()
add_definitions(-DFK_SERVER_ONLY)

# add_compile_options(-Wall)
set(CMAKE_COLOR_DIAGNOSTICS ON)

find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(spdlog REQUIRED)
find_package(cJSON REQUIRED)
find_package(PkgConfig)
pkg_search_module(libgit2 REQUIRED libgit2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(src)

add_executable(freekill-asio)

add_subdirectory(src)
