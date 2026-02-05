# Copyright (c) Maia

add_test(
  NAME integration_tests_python
  COMMAND
    "${Python_EXECUTABLE}" -m pytest "${CMAKE_CURRENT_SOURCE_DIR}/integration"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
