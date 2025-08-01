find_package(Python COMPONENTS Interpreter Development)

set(SETUP_PY_IN "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in")
set(SETUP_PY    "${CMAKE_CURRENT_BINARY_DIR}/setup.py")

set(VERSION_PY_IN "${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/version.py.in")
set(VERSION_PY_OUT    "${CMAKE_CURRENT_BINARY_DIR}/src/xstudio/version.py")
set(VERSION_PY    "${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/version.py")

file(GLOB DEPS
      ${CMAKE_CURRENT_SOURCE_DIR}/*.in
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/*.py
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/api/*.py
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/api/auxiliary/*.py
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/api/auxiliary/*/*.py
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/api/intrinsic/*.py
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/api/session/*.py
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/api/*/*/*.py
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/api/*/*/*/*.py
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/cli/*.py
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/demo/*.py
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/common_api/*.py
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/connection/*.py
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/gui/*.py
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/plugin/*.py
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/sync_api/*.py
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/core/*.py
)

set(OUTPUT "${CMAKE_BINARY_DIR}/bin/python")

set(PYTHONVP "python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}")

configure_file(${SETUP_PY_IN} ${SETUP_PY})
configure_file(${VERSION_PY_IN} ${VERSION_PY_OUT} @ONLY)
configure_file(${VERSION_PY_IN} ${VERSION_PY} @ONLY)

add_custom_command(OUTPUT ${OUTPUT}/lib/${PYTHONVP}/site-packages/xstudio/api
                   COMMAND ${Python_EXECUTABLE}
                   ARGS setup.py install --old-and-unmanageable --prefix=${OUTPUT}
                   DEPENDS ${DEPS})

add_custom_target(python_module ALL DEPENDS __pybind_xstudio ${OUTPUT}/lib/${PYTHONVP}/site-packages/xstudio/api)


if(INSTALL_PYTHON_MODULE)
  install(DIRECTORY ${OUTPUT}/lib/${PYTHONVP}/site-packages/xstudio
          DESTINATION lib/python/)

  install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio-control
          DESTINATION bin)
  install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio-inject
          DESTINATION bin)
  install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudiopy
          DESTINATION bin)
endif()

if(WIN32)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio DESTINATION ${CMAKE_INSTALL_PREFIX}/python/ FILES_MATCHING PATTERN "*.py")
endif()

# install(CODE "execute_process(COMMAND ${PYTHON} ${SETUP_PY} install)")

# to run python tests we need xstudio and python.
if (BUILD_TESTING AND INSTALL_XSTUDIO)
  add_subdirectory(test)
endif()
