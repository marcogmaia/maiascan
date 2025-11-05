# Copyright (c) Maia

add_library(application)

target_sources(
  application
  PUBLIC
    FILE_SET HEADERS
      FILES
        process_model.h
        process_selector_presenter.h
        scanner_presenter.h
        scan_result_model.h
  PRIVATE
    process_selector_presenter.cpp
    scanner_presenter.cpp
    scan_result_model.cpp
)

target_link_libraries(application PUBLIC core)
