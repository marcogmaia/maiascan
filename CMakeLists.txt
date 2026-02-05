# Copyright (c) Maia

cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(PythonUtils)
create_python_venv("3.12")

project(
  maiascan
  VERSION 0.0.1
  LANGUAGES
    C
    CXX
)

# TODO: remove this from here. This is for sccache.

set(
  CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
  "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>"
)
cmake_policy(SET CMP0141 NEW)

find_package(GTest CONFIG REQUIRED)

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
