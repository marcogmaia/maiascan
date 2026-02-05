# Copyright (c) Maia

function(create_python_venv PYTHON_VERSION)
  find_program(UV_EXECUTABLE uv REQUIRED)

  set(VENV_DIR "${CMAKE_BINARY_DIR}/.venv")

  # Force UV to create venv in build directory
  set(ENV{UV_PROJECT_ENVIRONMENT} "${VENV_DIR}")

  # Sync environment (creates venv if needed)
  # We use 'uv sync' which reads pyproject.toml
  if(EXISTS "${CMAKE_SOURCE_DIR}/pyproject.toml")
    message(STATUS "Syncing Python virtual environment with uv...")
    if(EXISTS "${CMAKE_SOURCE_DIR}/uv.lock")
      set(
        UV_SYNC_ARGS
        sync
        --frozen
        --python
        ${PYTHON_VERSION}
      )
    else()
      message(STATUS "uv.lock not found, generating one...")
      set(
        UV_SYNC_ARGS
        sync
        --python
        ${PYTHON_VERSION}
      )
    endif()

    execute_process(
      COMMAND
        ${UV_EXECUTABLE} ${UV_SYNC_ARGS}
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      RESULT_VARIABLE UV_SYNC_RESULT
    )
    if(NOT UV_SYNC_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to sync Python environment with uv")
    endif()
  else()
    message(FATAL_ERROR "pyproject.toml not found in ${CMAKE_SOURCE_DIR}")
  endif()

  # Determine Python executable path inside venv
  if(WIN32)
    set(PYTHON_VENV_EXECUTABLE "${VENV_DIR}/Scripts/python.exe")
  else()
    set(PYTHON_VENV_EXECUTABLE "${VENV_DIR}/bin/python")
  endif()

  # # Set hints for FindPython
  # set(Python_ROOT_DIR "${VENV_DIR}" CACHE PATH "" FORCE)

  set(
    Python_EXECUTABLE
    "${PYTHON_VENV_EXECUTABLE}"
    CACHE FILEPATH
    "Venv Interpreter"
    FORCE
  )

  unset(Python_ROOT_DIR CACHE)
  set(ENV{VIRTUAL_ENV} "${VENV_DIR}")

  # Re-run CMake if pyproject.toml changes
  set_property(
    DIRECTORY
    APPEND
    PROPERTY
      CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/pyproject.toml"
  )

  message(STATUS "Python virtual environment ready at: ${VENV_DIR}")
endfunction()
