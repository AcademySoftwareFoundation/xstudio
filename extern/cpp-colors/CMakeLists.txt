cmake_minimum_required(VERSION 2.8.4)
project(cpp_colors)

enable_testing()

set(WARNINGS)

# Platform
if(UNIX)
  option(BUILD_TESTS "Build tests." ON)
  option(BUILD_EXAMPLES "Build examples." ON)
elseif(WIN32)
  option(BUILD_TESTS "Build tests." ON)
  option(BUILD_EXAMPLES "Build examples." ON)

  add_definitions(-DUNICODE -D_UNICODE)
else()
  message(FATAL_ERROR "-- Unsupported Build Platform.")
endif()

# Compiler
if(${CMAKE_CXX_COMPILER_ID} STREQUAL Clang)
  message("-- Setting clang options")

  set(WARNINGS "-Wall -Wextra")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-strict-aliasing")
  set(STRICT_CXX_FLAGS ${WARNINGS} "-Werror -pedantic")
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL GNU)
  message("-- Setting gcc options")

  set(WARNINGS "-Wall -Wextra")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-strict-aliasing")
  set(STRICT_CXX_FLAGS ${WARNINGS} "-Werror -pedantic")
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
  message("-- Setting msvc options")
else()
  message("-- Unknown compiler, success is doubtful.")
endif()

# Reconfigure final output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

include_directories(include)

if(BUILD_TESTS)
  add_subdirectory(test)
endif()

if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# Clean the entire build directory
add_custom_target(clean-all
 COMMAND ${CMAKE_BUILD_TOOL} clean
 COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/../clean-all.cmake
)
