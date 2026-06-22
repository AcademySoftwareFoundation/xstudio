
include(CTest)

find_package(Python COMPONENTS Interpreter Development)
find_package(pybind11 REQUIRED)  # or `add_subdirectory(pybind11)`
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

add_executable(embedded_python_test embedded_python_test.cpp)
default_options_gtest(embedded_python_test)
target_compile_options(embedded_python_test
	PRIVATE -fvisibility=hidden
)

target_link_libraries(embedded_python_test
	PRIVATE
		xstudio::embedded_python
		pybind11::embed
		xstudio::utility
		xstudio::media
		${GTEST_LDFLAGS}
)
add_test(embedded_python_tests embedded_python_test)

add_executable(embedded_python_actor_test embedded_python_actor_test.cpp)
default_options_gtest(embedded_python_actor_test)
target_compile_options(embedded_python_actor_test
	PRIVATE -fvisibility=hidden
)
target_link_libraries(embedded_python_actor_test
	PRIVATE
		xstudio::embedded_python
		pybind11::embed
		xstudio::utility
		xstudio::media
		${GTEST_LDFLAGS}
)
add_test(embedded_python_tests embedded_python_actor_test)

set_target_properties(embedded_python_actor_test PROPERTIES LINK_DEPENDS_NO_SHARED true)
set_target_properties(embedded_python_test PROPERTIES LINK_DEPENDS_NO_SHARED true)