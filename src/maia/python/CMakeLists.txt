# We want to use the Python interpreter from the virtual environment
# if(WIN32)
#   set(Python_EXECUTABLE "${Python_ROOT_DIR}/Scripts/python.exe" CACHE FILEPATH "Python interpreter" FORCE)
# endif()

find_package(pybind11 CONFIG REQUIRED)
find_package(Python REQUIRED COMPONENTS Interpreter)

# Find site-packages in the venv
execute_process(
  COMMAND
    "${Python_EXECUTABLE}" -c
    "import sysconfig; print(sysconfig.get_path('purelib'))"
  OUTPUT_VARIABLE PYTHON_SITE_PACKAGES
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

pybind11_add_module(py_maiascan bindings.cpp)

set_target_properties(
  py_maiascan
  PROPERTIES
    OUTPUT_NAME
      "maiascan"
)

target_link_libraries(
  py_maiascan
  PRIVATE
    core
    application
)

# Register the module in the venv by creating a .pth file
# We use a config-specific filename to support multi-config generators (like VS)
file(
  GENERATE OUTPUT "${PYTHON_SITE_PACKAGES}/maiascan-$<CONFIG>.pth"
  CONTENT "$<TARGET_FILE_DIR:py_maiascan>"
)

add_custom_command(
  TARGET py_maiascan
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E env "PYTHONPATH=$<TARGET_FILE_DIR:py_maiascan>"
    "${Python_EXECUTABLE}" -m pybind11_stubgen maiascan --output-dir
    "$<TARGET_FILE_DIR:py_maiascan>" || ${CMAKE_COMMAND} -E echo
    "Warning: pybind11_stubgen failed or not found, skipping stubs."
  COMMENT "Generating Python type stubs for py_maiascan..."
  VERBATIM
)
