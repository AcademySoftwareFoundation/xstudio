find_package(Python COMPONENTS Interpreter Development)

set(SETUP_PY_IN "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in")
set(SETUP_PY    "${CMAKE_CURRENT_BINARY_DIR}/setup.py")

set(VERSION_PY_IN "${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/version.py.in")
set(VERSION_PY_OUT    "${CMAKE_CURRENT_BINARY_DIR}/src/xstudio/version.py")
set(VERSION_PY    "${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio/version.py")

file(GLOB_RECURSE DEPS ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio *.py)

set(PYTHONVP "python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}")

if(APPLE)

    set(VCPKG_LOCATION "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")

    configure_file(${SETUP_PY_IN} ${SETUP_PY})
    configure_file(${VERSION_PY_IN} ${VERSION_PY_OUT} @ONLY)
    configure_file(${VERSION_PY_IN} ${VERSION_PY} @ONLY)

    add_custom_command(OUTPUT ${VCPKG_LOCATION}/xstudio/api
                    COMMAND ${Python_EXECUTABLE}
                    ARGS setup.py install --old-and-unmanageable --prefix=${VCPKG_LOCATION}
                    DEPENDS ${DEPS})

    add_custom_target(python_module ALL DEPENDS __pybind_xstudio ${VCPKG_LOCATION}/xstudio/api)

    # Here we copy the entire python installation into Frameworks
    install(DIRECTORY ${VCPKG_LOCATION}/lib/${PYTHONVP} DESTINATION ${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/Frameworks/lib/)

else()

    set(OUTPUT "${CMAKE_BINARY_DIR}/bin/python")

    configure_file(${SETUP_PY_IN} ${SETUP_PY})
    configure_file(${VERSION_PY_IN} ${VERSION_PY_OUT} @ONLY)
    configure_file(${VERSION_PY_IN} ${VERSION_PY} @ONLY)

    if (WIN32)
        # Stage the xstudio Python package directly into the dev-tree mirror of
        # the install layout. Runtime PYTHONHOME is build/bin/python3 in dev
        # (set up by src/embedded_python/src/CMakeLists.txt) and
        # <prefix>/bin/python3 at install, Python's standard site-discovery
        # resolves `import xstudio` from there.
        #
        # Replaces a previous `python -m pip install .` invocation that
        # implicitly mutated vcpkg's tools/python3/Lib/site-packages, then
        # relied on `install(DIRECTORY tools/python3 ...)` to propagate xstudio 
        # into the installer.  Now the build-tree stage is explicit and the 
        # install rule below is explicit.
        # cmake -E copy_directory_if_different is incremental — only changed
        # .py files are copied on rebuilds.
        set(WIN_PYTHON_SITE "${CMAKE_BINARY_DIR}/bin/python3/Lib/site-packages/xstudio")
        add_custom_command(OUTPUT ${WIN_PYTHON_SITE}/__init__.py
                    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                        ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio
                        ${WIN_PYTHON_SITE}
                    DEPENDS ${DEPS} ${VERSION_PY}
                    COMMENT "Staging xstudio Python package to ${WIN_PYTHON_SITE}")
        add_custom_target(python_module ALL DEPENDS __pybind_xstudio ${WIN_PYTHON_SITE}/__init__.py)
    else()
        add_custom_command(OUTPUT ${OUTPUT}/lib/${PYTHONVP}/site-packages/xstudio/api
                    COMMAND ${Python_EXECUTABLE}
                    ARGS setup.py install --old-and-unmanageable --prefix=${OUTPUT}
                    DEPENDS __pybind_xstudio ${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in)
        add_custom_target(python_module ALL DEPENDS __pybind_xstudio ${OUTPUT}/lib/${PYTHONVP}/site-packages/xstudio/api)
    endif()

endif()


if(NOT APPLE)
    if(INSTALL_PYTHON_MODULE)

        if (NOT WIN32)
            install(DIRECTORY ${OUTPUT}/lib/${PYTHONVP}/site-packages/xstudio
                DESTINATION lib/python/)
        else()
            # Install the staged xstudio Python package to its final install
            # location. WIN_PYTHON_SITE is set above to
            # ${CMAKE_BINARY_DIR}/bin/python3/Lib/site-packages/xstudio.
            # Previously no explicit Windows install rule existed for the
            # xstudio .py files — they reached the installer only as a
            # side-effect of `pip install .` mutating vcpkg's tree.
            install(DIRECTORY ${WIN_PYTHON_SITE}
                DESTINATION bin/python3/Lib/site-packages)
        endif()

        if(WIN32)
            # Windows: install .cmd wrappers (extensionless scripts don't work on Windows)
            install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio-control.cmd
                DESTINATION bin)
            install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio-inject.cmd
                DESTINATION bin)
            install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudiopy.cmd
                DESTINATION bin)
        else()
            # Linux/macOS: install shell scripts
            install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio-control
                DESTINATION bin)
            install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudio-inject
                DESTINATION bin)
            install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/src/xstudiopy
                DESTINATION bin)
        endif()
     endif()
endif()

# to run python tests we need xstudio and python.
if (BUILD_TESTING AND INSTALL_XSTUDIO)
  add_subdirectory(test)
endif()
