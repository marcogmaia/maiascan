# Copyright (c) Maia

set(CMAKE_MFC_FLAG 2)

add_library(scanner)

target_sources(
  scanner
  PUBLIC
    FILE_SET HEADERS
      FILES live_process_accessor.h scanner.h
  PRIVATE
    scanner.cpp
    live_process_accessor.cpp
)

target_link_libraries(scanner PUBLIC base)

if(WIN32)
  target_compile_definitions(
    scanner
    PUBLIC
      _AFXDLL
      _WIN32_WINNT=0x0A00 # Target Windows 10
      WINVER=0x0A00 # Also set WINVER for compatibility
  )
endif()
