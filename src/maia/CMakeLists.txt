# Copyright (c) Maia

add_subdirectory(application)
add_subdirectory(core)
add_subdirectory(gui)
add_subdirectory(mmem)
add_subdirectory(python)
add_subdirectory(tests)

find_package(spdlog CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(libassert CONFIG REQUIRED)

add_library(base)
target_sources(
  base
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS "${MAIA_BASE_DIR}"
      FILES ./logging.h ./assert.h
  PRIVATE
    assert.cpp
)

target_link_libraries(
  base
  PUBLIC
    spdlog::spdlog
    EnTT::EnTT
    libassert::assert
)

if(MSVC)
  target_compile_definitions(
    base
    PUBLIC
      WIN32_LEAN_AND_MEAN
      NOMINMAX
  )
endif()

add_executable(maiascan)

target_sources(
  maiascan
  PRIVATE
    main.cpp
    assets/maiascan.rc
)

target_link_libraries(
  maiascan
  PRIVATE
    base
    gui
    core
    application
)
