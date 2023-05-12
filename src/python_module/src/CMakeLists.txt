project(__pybind_xstudio VERSION 0.1.0 LANGUAGES CXX)

find_package(pybind11 REQUIRED)
find_package(Python COMPONENTS Interpreter)

set(PYTHONVP "python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}")

add_library(
	${PROJECT_NAME}
	MODULE
	py_atoms.cpp
	py_context.cpp
	py_link.cpp
	py_messages.cpp
	py_playhead.cpp
	py_plugin.cpp
	py_register.cpp
	py_remote_session_file.cpp
	py_types.cpp
	py_ui.cpp
	py_utility.cpp
	py_xstudio.cpp
)
add_library(xstudio::python_module ALIAS ${PROJECT_NAME})

default_options_local(${PROJECT_NAME})

target_link_libraries(
	${PROJECT_NAME}
	PUBLIC
		pybind11::module
		xstudio::utility_static
		xstudio::global_store_static
		xstudio::json_store_static
		xstudio::timeline_static
		caf::core
		caf::io
)

if(INSTALL_XSTUDIO)
	set_target_properties(
		${PROJECT_NAME}
		PROPERTIES
		PREFIX "${PYTHON_MODULE_PREFIX}"
	    SUFFIX "${PYTHON_MODULE_EXTENSION}"
	    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/python/lib/${PYTHONVP}/site-packages/xstudio/core"
	)
else()
	set_target_properties(
		${PROJECT_NAME}
		PROPERTIES
		PREFIX "${PYTHON_MODULE_PREFIX}"
	    SUFFIX "${PYTHON_MODULE_EXTENSION}"
	)

	install(TARGETS ${PROJECT_NAME}
	        LIBRARY DESTINATION lib/python/xstudio/core)

endif(INSTALL_XSTUDIO)


