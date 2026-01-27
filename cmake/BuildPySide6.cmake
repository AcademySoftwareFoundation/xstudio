include(FetchContent)

# URL and Hash for PySide6 6.5.3 matching OpenRV
FetchContent_Declare(
  pyside6_source
  URL https://mirrors.ocf.berkeley.edu/qt/official_releases/QtForPython/pyside6/PySide6-6.5.3-src/pyside-setup-everywhere-src-6.5.3.zip
  URL_HASH MD5=515d3249c6e743219ff0d7dd25b8c8d8
  PATCH_COMMAND sed -i.bak "s/check_allowed_python_version()/# check_allowed_python_version()/" setup.py
)

# FetchContent_MakeAvailable(pyside6_source) # This adds it as subproject which we don't want

FetchContent_GetProperties(pyside6_source)
if(NOT pyside6_source_POPULATED)
  FetchContent_Populate(pyside6_source)
endif()

# We install PySide6 into a local directory in the build tree
set(PYSIDE6_INSTALL_DIR ${CMAKE_BINARY_DIR}/python)
# Ensure this directory exists
file(MAKE_DIRECTORY ${PYSIDE6_INSTALL_DIR})

# We need to find the root of Qt installation to pass to the script
# Qt6_DIR points to lib/cmake/Qt6. Use cmake path command to traverse up.
# or simply assume standard layout if we can't deduce it.
# Actually make_pyside6.py expects --qt-dir to contain bin/qtpaths.
# If Qt6_DIR is .../lib/cmake/Qt6, then root is .../../../..
get_filename_component(_qt6_lib_cmake_dir ${Qt6_DIR} DIRECTORY) # .../lib/cmake
get_filename_component(_qt6_lib_dir ${_qt6_lib_cmake_dir} DIRECTORY) # .../lib
get_filename_component(_qt6_root_dir ${_qt6_lib_dir} DIRECTORY) # .../ (root)

# Check if qtpaths exists there to be sure
if(EXISTS "${_qt6_root_dir}/bin/qtpaths" OR EXISTS "${_qt6_root_dir}/bin/qtpaths.exe")
    set(QT_ROOT_DIR ${_qt6_root_dir})
else()
    # It might be in a different layout (e.g. system install vs local install)
    # Qt6_DIR usually works.
    message(STATUS "Could not deduce Qt root from Qt6_DIR=${Qt6_DIR}. Probing...")
    # Try finding qtpaths directly
    find_program(QTPATHS_EXECUTABLE NAMES qtpaths qtpaths6 HINTS ${_qt6_root_dir}/bin)
    if (QTPATHS_EXECUTABLE)
        get_filename_component(_qtpaths_dir ${QTPATHS_EXECUTABLE} DIRECTORY)
        get_filename_component(QT_ROOT_DIR ${_qtpaths_dir} DIRECTORY)
    else()
        message(FATAL_ERROR "Could not find qtpaths executable. PySide6 build requires it.")
    endif()
endif()

message(STATUS "Building PySide6 6.5.3 using Qt from ${QT_ROOT_DIR}")

# We predict the location of the libraries based on the install structure.
# setup.py install usually goes to lib/pythonX.Y/site-packages
set(PYTHON_SITE_PACKAGES "lib/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}/site-packages")
# Use Python3_VERSION if Python_VERSION is not set (sometimes FindPython3 is used)
if (NOT Python_VERSION_MAJOR AND Python3_VERSION_MAJOR)
    set(PYTHON_SITE_PACKAGES "lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/site-packages")
endif()

set(PYSIDE6_LIB_DIR "${PYSIDE6_INSTALL_DIR}/${PYTHON_SITE_PACKAGES}/PySide6")
set(SHIBOKEN6_LIB_DIR "${PYSIDE6_INSTALL_DIR}/${PYTHON_SITE_PACKAGES}/shiboken6")

message(STATUS "BuildPySide6: Expecting output at ${PYSIDE6_LIB_DIR}")


# Library extension
if(APPLE)
    set(LIB_EXT ".dylib")
    set(SHARED_LIB_EXT ".so") # Python modules are .so
    set(PYSIDE6_LIB_NAME "libpyside6.abi3.6.5${LIB_EXT}")
    set(SHIBOKEN6_LIB_NAME "libshiboken6.abi3.6.5${LIB_EXT}")
elseif(UNIX)
    set(LIB_EXT ".so")
    set(PYSIDE6_LIB_NAME "libpyside6.abi3${LIB_EXT}")
    set(SHIBOKEN6_LIB_NAME "libshiboken6.abi3${LIB_EXT}")
elseif(WIN32)
    set(LIB_EXT ".lib")
    set(PYSIDE6_LIB_NAME "pyside6.lib") # verify windows names
    set(SHIBOKEN6_LIB_NAME "shiboken6.lib")
endif()

# Define the custom command to build PySide6
add_custom_command(
  OUTPUT ${PYSIDE6_LIB_DIR}/__init__.py
         ${PYSIDE6_LIB_DIR}/${PYSIDE6_LIB_NAME}
         ${SHIBOKEN6_LIB_DIR}/${SHIBOKEN6_LIB_NAME}
  COMMAND ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/src/build/make_pyside6.py
          --prepare
          --build
          --source-dir ${pyside6_source_SOURCE_DIR}
          --qt-dir ${QT_ROOT_DIR}
          --python-executable ${Python_EXECUTABLE}
          --temp-dir ${CMAKE_BINARY_DIR}/pyside6_build_temp
          --variant ${CMAKE_BUILD_TYPE}
          --output-dir ${PYSIDE6_INSTALL_DIR}
  COMMAND ${CMAKE_COMMAND} -E create_symlink ../shiboken6/${SHIBOKEN6_LIB_NAME} ${PYSIDE6_LIB_DIR}/${SHIBOKEN6_LIB_NAME}
  COMMENT "Building PySide6 6.5.3 to ${PYSIDE6_LIB_DIR}..."
  VERBATIM
)

# Use checking __init__.py as marker for existence
add_custom_target(pyside6_local_build ALL DEPENDS ${PYSIDE6_LIB_DIR}/__init__.py)

# PySide6 Target

add_library(PySide6::pyside6 SHARED IMPORTED GLOBAL)
set_target_properties(PySide6::pyside6 PROPERTIES
    IMPORTED_LOCATION "${PYSIDE6_LIB_DIR}/${PYSIDE6_LIB_NAME}"
    INTERFACE_INCLUDE_DIRECTORIES "${PYSIDE6_LIB_DIR}/include"
)
add_dependencies(PySide6::pyside6 pyside6_local_build)

set(SHIBOKEN6_GENERATOR_INCLUDE_DIR "${PYSIDE6_INSTALL_DIR}/${PYTHON_SITE_PACKAGES}/shiboken6_generator/include")

# Shiboken6 Target
add_library(Shiboken6::libshiboken SHARED IMPORTED GLOBAL)
set_target_properties(Shiboken6::libshiboken PROPERTIES
    IMPORTED_LOCATION "${SHIBOKEN6_LIB_DIR}/${SHIBOKEN6_LIB_NAME}"
    INTERFACE_INCLUDE_DIRECTORIES "${SHIBOKEN6_LIB_DIR}/include;${SHIBOKEN6_GENERATOR_INCLUDE_DIR}"
)
add_dependencies(Shiboken6::libshiboken pyside6_local_build)

# Set variables used by downstream (legacy support if targets aren't enough)
set(PySide6_INCLUDE_DIR "${PYSIDE6_LIB_DIR}/include")
set(SHIBOKEN6_INCLUDE_DIRS "${SHIBOKEN6_LIB_DIR}/include")
set(PySide6_LIBRARIES PySide6::pyside6)
set(Shiboken6_LIBRARIES Shiboken6::libshiboken)

# Ensure these directories exist so CMake doesn't complain about non-existent include paths
# They will be populated during the build step.
file(MAKE_DIRECTORY "${PySide6_INCLUDE_DIR}")
file(MAKE_DIRECTORY "${SHIBOKEN6_INCLUDE_DIRS}")
file(MAKE_DIRECTORY "${SHIBOKEN6_GENERATOR_INCLUDE_DIR}")

# Install PySide6 and Shiboken6 into the app bundle Resources/python/lib
if(APPLE)
    install(DIRECTORY "${PYSIDE6_INSTALL_DIR}/${PYTHON_SITE_PACKAGES}/PySide6" 
            DESTINATION Resources/python/lib)
    install(DIRECTORY "${PYSIDE6_INSTALL_DIR}/${PYTHON_SITE_PACKAGES}/shiboken6" 
            DESTINATION Resources/python/lib)
    # Explicitly install __init__.py to ensure it is present
    install(FILES "${PYSIDE6_LIB_DIR}/__init__.py" DESTINATION Resources/python/lib/PySide6)
endif()
