project(__pybind_xstudio VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

find_package(Python COMPONENTS Interpreter Development)
find_package(pybind11 CONFIG)
find_package(Python COMPONENTS Interpreter)

set(PYTHONVP "python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}")

if(WIN32)
  add_compile_options("/bigobj")
  set(PYTHON_MODULE_EXTENSION .pyd)
endif()

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

set_python_to_proper_build_type()

if (APPLE)
	# Note - for Apple we install into the vcpkg installation structure at build time.
	# Our install commands will copy the entire python3.X folder into the app bundle
	# when making the re-locatable .app structure
	set(OUTPUT_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/${PYTHONVP}/site-packages/xstudio/core")
else()
	set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/python/lib/${PYTHONVP}/site-packages/xstudio/core")
endif()

target_link_libraries(
	${PROJECT_NAME}
	PUBLIC
		pybind11::module
		xstudio::utility_static
		xstudio::global_store_static
		xstudio::json_store_static
		xstudio::timeline_static
		xstudio::media_static
		CAF::core
		CAF::io
)

if (APPLE)
	# No idea how/why PYTHON_MODULE_EXTENSION is set to what it is, but on
	# MacOS lately it will be .cpython-39-darwin.so and this means the
	# module will not import!
	set(PYTHON_MODULE_EXTENSION ".so")
endif()

if(INSTALL_XSTUDIO)
	set_target_properties(
		${PROJECT_NAME}
		PROPERTIES
		PREFIX "${PYTHON_MODULE_PREFIX}"
	    SUFFIX "${PYTHON_MODULE_EXTENSION}"
	    LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR}
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

if(WIN32)
install(TARGETS ${PROJECT_NAME} DESTINATION bin/python3/Lib/site-packages/xstudio/core)
endif()
