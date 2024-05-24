cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

option(USE_VCPKG "Use Vcpkg for package management" OFF)
if(WIN32)
    set(USE_VCPKG ON)
    set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Ob0 /Od /Oy-") 
    add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

endif()

if (USE_VCPKG)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/vcpkg.cmake)
endif()

set(XSTUDIO_GLOBAL_VERSION "0.11.2" CACHE STRING "Version string")
set(XSTUDIO_GLOBAL_NAME xStudio)

project(${XSTUDIO_GLOBAL_NAME} VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

cmake_policy(VERSION 3.26)

option(BUILD_TESTING "Build tests" OFF)
option(INSTALL_PYTHON_MODULE "Install python module" ON)
option(INSTALL_XSTUDIO "Install xstudio" ON)
option(BUILD_DOCS "Build xStudio documentation" ON)
option(ENABLE_CLANG_TIDY "Enable clang-tidy, ninja clang-tidy." OFF)
option(ENABLE_CLANG_FORMAT "Enable clang format, ninja clangformat." OFF)
option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." TRUE)
option(OPTIMIZE_FOR_NATIVE "Build with -march=native" OFF)
option(BUILD_RESKIN "Build xstudio reskin binary" ON)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

set(STUDIO_PLUGINS "" CACHE STRING "Enable compilation of SITE plugins")


if (("${CMAKE_GENERATOR}" MATCHES "Makefiles" OR ("${CMAKE_GENERATOR}" MATCHES "Ninja" AND NOT WIN32)) AND NOT __CHECKED_CXX_FLAGS)
    set(__CHECKED_CXX_FLAGS TRUE CACHE INTERNAL "Whether we checked the CXX flags already")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${COMMON_GCC}" CACHE STRING "" FORCE)
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${COMMON_GCC}" CACHE STRING "" FORCE)
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${COMMON_GCC}" CACHE STRING "" FORCE)
	set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${COMMON_GCC}" CACHE STRING "" FORCE)
	unset(COMMON_GCC)
endif()

set(CXXOPTS_BUILD_TESTS OFF CACHE BOOL "Enable or disable cxxopts' tests")
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpic -fmax-errors=5 -fdiagnostics-color=always")
endif()

if (${OPTIMIZE_FOR_NATIVE})
  include(CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
  if(COMPILER_SUPPORTS_MARCH_NATIVE)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
  endif()
endif()

set(GCC_MARCH_OVERRIDE "" CACHE STRING "Override -march")

if (NOT ${GCC_MARCH_OVERRIDE} STREQUAL "")
  include(CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG("-march=${GCC_MARCH_OVERRIDE}" COMPILER_SUPPORTS_MARCH)
  if(COMPILER_SUPPORTS_MARCH)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=${GCC_MARCH_OVERRIDE}")
  endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU") 
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fpic")
endif()

set(TEST_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/test_resource")
set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
  set(CMAKE_CXX_STANDARD 20)
  add_compile_definitions($<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)
else()
  set(CMAKE_CXX_STANDARD 17)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_LINK_DEPENDS_NO_SHARED true)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set(CMAKE_THREAD_LIBS_INIT "-lpthread")
endif()
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)

set(REPROC_INSTALL OFF)
set(REPROC++ ON)

set(OpenGL_GL_PREFERENCE GLVND)

if (USE_SANITIZER STREQUAL "Address")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fsanitize=address")
elseif (USE_SANITIZER STREQUAL "Thread")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
elseif (USE_SANITIZER STREQUAL "Undefined")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
elseif (USE_SANITIZER STREQUAL "Leak")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=leak")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=leak")
elseif (USE_SANITIZER STREQUAL "Memory")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=memory")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=memory")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fsanitize=memory")
endif()

# This should turn on coloured shell output for ninja
if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
       add_compile_options (-fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       add_compile_options (-fcolor-diagnostics)
    endif ()
endif ()

if(ENABLE_CLANG_TIDY)
  if(CMake_SOURCE_DIR STREQUAL CMake_BINARY_DIR)
    message(FATAL_ERROR "CMake_RUN_CLANG_TIDY requires an out-of-source build!")
  endif()

  find_program(CLANG_TIDY_COMMAND NAMES run-clang-tidy.py)
  if(NOT CLANG_TIDY_COMMAND)
    message(WARNING "ENABLE_CLANG_TIDY is ON but run-clang-tidy.py is not found!")
  else()
    configure_file(.clang-tidy .clang-tidy)
    set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
    set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")
    add_custom_target(
        clang-tidy
        COMMAND ${CLANG_TIDY_COMMAND} -quiet -extra-arg=-I${CLANG_PATH}/lib/clang/11.1.0/include
        USES_TERMINAL
    )
  endif()

endif()

if(WIN32)
    ADD_DEFINITIONS(-DNOMINMAX)
endif()

if(MSVC)
  #Getenv complains, would be good to fix later but tired of seeing this for now.
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# Add the necessary libraries from Vcpkg if Vcpkg integration is enabled
if(USE_VCPKG)
  set(CMAKE_CXX_STANDARD 20)
  add_compile_options(/permissive-)
  set(VCPKG_INTEGRATION ON)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
  # Set Python in VCPKG
  set(Python_EXECUTABLE "${VCPKG_DIRECTORY}/../vcpkg_installed/x64-windows/tools/python3/python.exe")  
  # Install pip and sphinx
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "PATH=${VCPKG_DIRECTORY}/../vcpkg_installed/x64-windows/tools/python3" python.exe -m ensurepip --upgrade
    RESULT_VARIABLE ENSUREPIP_RESULT
  )
  if(ENSUREPIP_RESULT)
    message(FATAL_ERROR "Failed to ensurepip.")
  else()
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E env "PATH=${VCPKG_DIRECTORY}/../vcpkg_installed/x64-windows/tools/python3" python.exe -m pip install sphinx breathe sphinx-rtd-theme OpenTimelineIO importlib_metadata zipp
      RESULT_VARIABLE PIP_RESULT
    )
    if(PIP_RESULT)
      message(FATAL_ERROR "Failed to install Sphinx using pip.")
    endif()
  endif()
  # append vcpkg packages
  list(APPEND CMAKE_PREFIX_PATH "${VCPKG_DIRECTORY}/../vcpkg_installed/x64-windows")
  # enable UUID System Generator 
  add_definitions(-DUUID_SYSTEM_GENERATOR=ON)
  # build quickpromise
  add_subdirectory("extern/quickpromise")
  add_subdirectory("extern/quickfuture")

  # When moving to Qt6 or greater, we might be able to use qt_generate_deploy_app_script
  #set(deploy_script "${Qt5_DIR}/../../../windeployqt.exe  )
  
endif()

find_package(nlohmann_json CONFIG REQUIRED)
include(CTest)

if(ENABLE_CLANG_FORMAT)
  include (ClangFormat)
  file(GLOB_RECURSE FORMAT_ITEMS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/xstudio/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  )
  clangformat_setup(${FORMAT_ITEMS})
endif()

if(INSTALL_PYTHON_MODULE)
  add_subdirectory(python)
endif()

add_subdirectory(src)

if(INSTALL_XSTUDIO)
  add_subdirectory(share/preference)
  add_subdirectory(share/snippets)
  add_subdirectory(share/fonts)

  install(DIRECTORY include/xstudio
          DESTINATION include)

  INSTALL(DIRECTORY extern/ DESTINATION extern)

  if(BUILD_DOCS)
    if(NOT INSTALL_PYTHON_MODULE)
      add_subdirectory(python)
    endif ()
    add_subdirectory(docs)
  else()
    install(DIRECTORY share/docs/ DESTINATION share/xstudio/docs)
  endif ()

  include(CMakePackageConfigHelpers)

  configure_package_config_file(xStudioConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/xStudioConfig.cmake
    INSTALL_DESTINATION lib/cmake/${PROJECT_NAME}
  )
  write_basic_package_version_file("xStudioConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/xStudioConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/xStudioConfigVersion.cmake
      DESTINATION lib/cmake/${PROJECT_NAME}
  )

  install(EXPORT xstudio
    DESTINATION lib/cmake/${PROJECT_NAME}
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE xstudio::
    EXPORT_LINK_INTERFACE_LIBRARIES
  )

endif ()

add_subdirectory("extern/reproc")

if(USE_VCPKG)
  # To provide reliable ordering, we need to make this install script happen in a subdirectory.
  # Otherwise, Qt deploy will happen before we have the rest of the application deployed.
  add_subdirectory("scripts/qt_install")
endif()