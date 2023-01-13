include(CTest)
find_package(Python COMPONENTS Interpreter)
set(PYTHONVP "python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}")

add_test (NAME python_tests
  COMMAND ${PYTHON_EXECUTABLE} -m pytest -vv
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(python_tests
	PROPERTIES ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}/bin/python/lib/${PYTHONVP}/site-packages:$ENV{PYTHONPATH};TEST_RESOURCE=${TEST_RESOURCE}"
)
