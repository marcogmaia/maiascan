# Copyright (c) Maia

cmake_minimum_required(VERSION 4.0)

project(
  maiascan
  VERSION 0.0.1
  LANGUAGES
    C
    CXX
)

find_package(GTest REQUIRED)

find_package(CLI11 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(MFC REQUIRED)
find_package(spdlog REQUIRED)

set(MAIA_BASE_DIR "${CMAKE_SOURCE_DIR}/src/")
include(CTest)
include(GoogleTest)

add_subdirectory(src)
