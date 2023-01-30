cmake_minimum_required(VERSION 3.12)
project(embedded_python VERSION 0.1.0 LANGUAGES CXX)

find_package(pybind11 REQUIRED)  # or `add_subdirectory(pybind11)`
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)


set(SOURCES
	embedded_python.cpp
	embedded_python_actor.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(xstudio::embedded_python ALIAS ${PROJECT_NAME})
default_options(${PROJECT_NAME})
target_compile_options(${PROJECT_NAME}
	PRIVATE -fvisibility=hidden
)
target_link_libraries(${PROJECT_NAME}
	PUBLIC
		caf::core
	PRIVATE
		xstudio::utility
		xstudio::broadcast
		pybind11::embed
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)