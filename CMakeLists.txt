cmake_minimum_required(VERSION 3.20)
project(
  maiascan
  VERSION 0.0.1
  LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)

find_package(GTest REQUIRED)

find_package(CLI11 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(MFC REQUIRED)
find_package(spdlog REQUIRED)
find_package(tl-expected CONFIG REQUIRED)
find_package(tl-optional CONFIG REQUIRED)

include_directories("${CMAKE_SOURCE_DIR}/src")

include(CTest)
include(GoogleTest)

add_subdirectory(src)
