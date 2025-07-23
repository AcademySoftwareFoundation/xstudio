cmake_minimum_required(VERSION 3.28 FATAL_ERROR)
cmake_policy(VERSION 3.28)

set(XSTUDIO_GLOBAL_VERSION "1.0.0" CACHE STRING "Version string")
set(XSTUDIO_GLOBAL_NAME xStudio)

# set(CMAKE_OSX_DEPLOYMENT_TARGET "14.5" CACHE STRING "Minimum OS X deployment version" FORCE)

project(${XSTUDIO_GLOBAL_NAME} VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

option(BUILD_TESTING "Build tests" OFF)
option(INSTALL_PYTHON_MODULE "Install python module" ON)
option(INSTALL_XSTUDIO "Install xstudio" ON)
option(BUILD_DOCS "Build xStudio documentation" ON)
option(ENABLE_CLANG_TIDY "Enable clang-tidy, ninja clang-tidy." OFF)
option(ENABLE_CLANG_FORMAT "Enable clang format, ninja clangformat." OFF)
option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." TRUE)
option(OPTIMIZE_FOR_NATIVE "Build with -march=native" OFF)
option(BUILD_RESKIN "Build xstudio reskin binary" ON)
option(OTIO_SUBMODULE "Automatically build OpenTimelineIO as a submodule" OFF)
option(USE_VCPKG "Use Vcpkg for package management" OFF)
option(BUILD_PYSIDE_WIDGETS "Build xstudio player as PySide widget" OFF)

if(WIN32)
    set(USE_VCPKG ON)
    #include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/vcpkg.cmake)
    set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Ob0 /Od /Oy-")
    add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)
    # enable UUID System Generator
    add_definitions(-DUUID_SYSTEM_GENERATOR=ON)
endif()

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

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpic -fdiagnostics-color=always")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
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


set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fpic")

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
    if(MSVC)
        target_compile_options(<target> PUBLIC /fsanitize=address)
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fsanitize=address")
    endif()
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
    set(CMAKE_CXX_STANDARD 20)
    add_compile_options(/permissive-)

  # Workaround for C++ 20+ comparisons in nlohmann json
  # https://github.com/nlohmann/json/issues/3868#issuecomment-1563726354
  add_definitions(-DJSON_HAS_THREE_WAY_COMPARISON=OFF)

  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

  # When moving to Qt6 or greater, we might be able to use qt_generate_deploy_app_script
  #set(deploy_script "${Qt6_DIR}/../../../windeployqt.exe  )
endif()

if(MSVC)
  #Getenv complains, would be good to fix later but tired of seeing this for now.
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)

endif()

if (USE_VCPKG)

  # When building with VCPKG, we will use OTIO submodule
  set(OTIO_SUBMODULE true)
  add_subdirectory("extern/otio")

  set(VCPKG_INTEGRATION ON)

  # Install pip and sphinx
  find_package(Python COMPONENTS Interpreter Development)

  message("Python_RUNTIME_LIBRARY_DIRS ${Python_RUNTIME_LIBRARY_DIRS}")

  execute_process(
    COMMAND "${Python_EXECUTABLE}" -m ensurepip --upgrade
    RESULT_VARIABLE ENSUREPIP_RESULT
  )

  if(ENSUREPIP_RESULT)
    message(FATAL_ERROR "Failed to ensurepip.")
  else()
    execute_process(
      COMMAND "${Python_EXECUTABLE}" -m pip install setuptools sphinx breathe sphinx-rtd-theme OpenTimelineIO-Plugins importlib_metadata zipp
      RESULT_VARIABLE PIP_RESULT
    )
    if(PIP_RESULT)
      message(FATAL_ERROR "Failed to install Sphinx using pip.")
    endif()
  endif()

else()
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
  if(${OTIO_SUBMODULE})
	  add_subdirectory("extern/otio")
  endif()
 	find_package(PkgConfig REQUIRED)
endif()

# Add the necessary libraries from Vcpkg if Vcpkg integration is enabled

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

  # build quickpromise
  add_subdirectory(extern/quickpromise)
  add_subdirectory(extern/quickfuture)

  install(DIRECTORY include/xstudio
          DESTINATION include)

  INSTALL(DIRECTORY extern/ DESTINATION extern)

  add_subdirectory(share/preference)
  add_subdirectory(share/snippets)
  add_subdirectory(share/fonts)

  if(BUILD_DOCS)
    message("BUILDING DOCS")
    add_subdirectory(docs)
  else()
    if (APPLE)
      install(DIRECTORY share/docs/ DESTINATION ${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/Resources/docs)
    else()
      install(DIRECTORY share/docs/ DESTINATION share/xstudio/docs)
    endif()
  endif ()

  if (NOT APPLE)

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

  endif()

endif ()

if(USE_VCPKG)
  # To provide reliable ordering, we need to make this install script happen in a subdirectory.
  # Otherwise, Qt deploy will happen before we have the rest of the application deployed.
  add_subdirectory("scripts/qt_install")
endif()

if (WIN32)

  include(InstallRequiredSystemLibraries)

  set(CPACK_PACKAGE_VERSION "1.0.0")
  set(CPACK_PACKAGE_VERSION_MAJOR "1")
  set(CPACK_PACKAGE_VERSION_MINOR "0")
  set(CPACK_PACKAGE_VERSION_PATCH "0")
  set(CPACK_PACKAGE_VENDOR "DNEG / Academy Software Foundation")
  set (CPACK_NSIS_MUI_ICON
      "${CMAKE_CURRENT_SOURCE_DIR}/ui/icons\\\\xstudio_app.ico")
  set (CPACK_PACKAGE_NAME "xSTUDIO")
  set (CPACK_NSIS_PACKAGE_NAME "xSTUDIO")
  set (CPACK_PACKAGE_ICON
      "${CMAKE_CURRENT_SOURCE_DIR}/ui/icons\\\\xstudio_app.ico")
  set (CPACK_RESOURCE_FILE_LICENSE
      "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
  set (CPACK_CREATE_DESKTOP_LINKS xstudio)
  set (CPACK_NSIS_MENU_LINKS
      "share/docs/index.html"
      "xSTUDIO Help")

  if("${STUDIO_PLUGINS}" STREQUAL "dneg")
    # for dneg deployment, we add some CLI args to pick up our customised facility preferences thus:
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
      "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\xSTUDIO.lnk' '$INSTDIR\\\\bin\\\\xstudio.exe' '--pref N:\\\\SITE\\\\data\\\\xStudio\\\\dneg_defaults.json --pref N:\\\\SITE\\\\data\\\\xStudio\\\\windows_extra_config\\\\dneg_windows_defaults.json'"
    )
  else()
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
      "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\xSTUDIO.lnk' '$INSTDIR\\\\bin\\\\xstudio.exe'"
    )
  endif()

  #set (CPACK_PACKAGE_EXECUTABLES "xstudio" "xSTUDIO" )

  set (CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
      WriteRegStr HKCR '.xst' '' 'XStudioSession'
      WriteRegStr HKCR 'XStudioSession' '' 'xSTUDIO Session File'
      WriteRegStr HKCR 'XStudioSession\\\\shell' '' 'open'
      WriteRegStr HKCR 'XStudioSession\\\\DefaultIcon' \\\\
                        '' '$INSTDIR\\\\bin\\\\xstudio.exe,0'
      WriteRegStr HKCR 'XStudioSession\\\\shell\\\\open\\\\command' \\\\
                        '' '$INSTDIR\\\\bin\\\\xstudio.exe \\"%1\\"'
      WriteRegStr HKCR 'XStudioSession\\\\shell\\\\edit' \\\\
                        '' 'Edit xSTUDIO Session File'
      WriteRegStr HKCR 'XStudioSession\\\\shell\\\\edit\\\\command' \\\\
                        '' '$INSTDIR\\\\bin\\\\xstudio.exe \\"%1\\"'
      System::Call \\\\
        'Shell32::SHChangeNotify(i 0x8000000, i 0, i 0, i 0)'
      ")

  set (CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
      DeleteRegKey HKCR '.xst'
      DeleteRegKey HKCR 'XStudioSession'
      ")

  include(CPack)
endif()
