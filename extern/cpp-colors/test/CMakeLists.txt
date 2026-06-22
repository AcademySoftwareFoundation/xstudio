set(SOURCE_TEST_FILES
  suites/test_color.cpp
  run_tests.cpp)

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

find_package(Threads REQUIRED)

find_package(Boost)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(run_tests ${SOURCE_TEST_FILES})
target_link_libraries(run_tests ${GTEST_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

add_test(NAME all_tests
  WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  COMMAND run_tests
  )