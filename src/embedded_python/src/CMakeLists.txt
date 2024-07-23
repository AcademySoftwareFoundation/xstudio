cmake_minimum_required(VERSION 3.12)
project(embedded_python VERSION 0.1.0 LANGUAGES CXX)

find_package(pybind11 CONFIG REQUIRED)  # or `add_subdirectory(pybind11)`
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(Imath)
#find_package(OpenTime REQUIRED)
#find_package(OpenTimelineIO REQUIRED)

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

set_python_to_proper_build_type()

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		caf::core
	PRIVATE
		Imath::Imath
		xstudio::utility
		xstudio::broadcast
		pybind11::embed
		#OTIO::opentime
		#OTIO::opentimelineio
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

if(WIN32)
install(DIRECTORY ${VCPKG_DIRECTORY}/../vcpkg_installed/x64-windows/tools/python3/DLLs/ DESTINATION ${CMAKE_INSTALL_PREFIX}/python)
install(DIRECTORY ${VCPKG_DIRECTORY}/../vcpkg_installed/x64-windows/tools/python3/Lib/ DESTINATION ${CMAKE_INSTALL_PREFIX}/python)
install(FILES python310._pth DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif()