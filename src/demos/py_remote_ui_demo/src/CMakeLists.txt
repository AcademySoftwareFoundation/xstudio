project(py_remote_ui_demo VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

install(PROGRAMS
  ${CMAKE_CURRENT_SOURCE_DIR}/xStudioPythonRemoteUIDemo.py
  DESTINATION bin
  RENAME xStudioPythonRemoteUIDemo)