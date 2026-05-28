project(embedded_python VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

find_package(Python COMPONENTS Interpreter Development)
find_package(pybind11 CONFIG REQUIRED)  # or `add_subdirectory(pybind11)`
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(Imath)

find_package(Python REQUIRED COMPONENTS Interpreter Development)

set(SOURCES
	embedded_python.cpp
	embedded_python_actor.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(xstudio::embedded_python ALIAS ${PROJECT_NAME})
default_options(${PROJECT_NAME})

if(UNIX)
	target_compile_options(${PROJECT_NAME}
		PRIVATE -fvisibility=hidden
	)
endif()

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		CAF::core
	PRIVATE
		Imath::Imath
		xstudio::utility
		xstudio::global_store
		xstudio::media
		Python::Python
)
	

set_python_to_proper_build_type()

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

if(WIN32)
    set(_xstudio_vcpkg_python "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/python3")

    if(NOT EXISTS "${_xstudio_vcpkg_python}")
        message(FATAL_ERROR
            "vcpkg Python tree not found at ${_xstudio_vcpkg_python}. "
            "Re-run vcpkg install or check VCPKG_TARGET_TRIPLET.")
    endif()

    # Build-time mirror of vcpkg's tools/python3 into the build tree. Embedded
    # Python computes PYTHONHOME as <exe>/../../bin/python3 (see
    # embedded_python.cpp Windows branch), so a populated build/bin/python3 is
    # required to launch xstudio directly from the build tree with no
    # cmake --install. copy_directory_if_different is incremental: first build
    # ~150 MB, subsequent builds copy only changed files. The staged contents
    # (CPython interpreter, stdlib, vcpkg-bundled site-packages) are not
    # debugging targets for xstudio devs, so a copy has no debugger-ergonomic
    # downside.
    add_custom_target(stage_python_runtime ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${_xstudio_vcpkg_python}"
            "${CMAKE_BINARY_DIR}/bin/python3"
        COMMENT "Staging vcpkg Python runtime to ${CMAKE_BINARY_DIR}/bin/python3")

    # Install the entire python install from vcpkg to the bin/python3 folder
    install(DIRECTORY ${_xstudio_vcpkg_python} DESTINATION bin)
endif()