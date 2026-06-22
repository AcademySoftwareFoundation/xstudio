find_package(Python COMPONENTS Interpreter Development)

set(SETUP_PY_IN "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in")
set(SETUP_PY    "${CMAKE_CURRENT_BINARY_DIR}/setup.py")

file(GLOB_RECURSE DEPS ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio_pipequery *.py)

set(OUTPUT "${CMAKE_BINARY_DIR}/bin/python")

set(PYTHONVP "python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}")

configure_file(${SETUP_PY_IN} ${SETUP_PY})

add_custom_command(OUTPUT ${OUTPUT}/lib/${PYTHONVP}/site-packages/xstudio_pipequery
                   COMMAND ${Python_EXECUTABLE}
                   ARGS setup.py install --old-and-unmanageable --prefix=${OUTPUT}
                   DEPENDS ${DEPS} ${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in)

add_custom_target(pipequery_python_module ALL DEPENDS __pybind_xstudio ${OUTPUT}/lib/${PYTHONVP}/site-packages/xstudio_pipequery)



if(WIN32)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio_pipequery DESTINATION python/ FILES_MATCHING PATTERN "*.py")
else()
  if(INSTALL_PYTHON_MODULE)
    install(DIRECTORY ${OUTPUT}/lib/${PYTHONVP}/site-packages/xstudio_pipequery
            DESTINATION lib/python/)
  endif()
endif()
