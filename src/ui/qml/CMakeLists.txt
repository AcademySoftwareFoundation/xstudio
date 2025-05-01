


set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Quick Gui OpenGL OpenGLWidgets Test Widgets Concurrent REQUIRED)

if(WIN32)
else()
	find_package(Qt6 COMPONENTS DBus REQUIRED)
endif()

configure_file(.clang-tidy .clang-tidy)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpic")
    set(DCMAKE_EXE_LINKER_FLAGS "${DCMAKE_EXE_LINKER_FLAGS} -fpic")
endif()
# QT5_ADD_RESOURCES(PROTOTYPE_RCS)
# if (Qt6_POSITION_INDEPENDENT_CODE)
#   SET(CMAKE_POSITION_INDEPENDENT_CODE ON)
# endif()

add_src_and_test(bookmark)
add_src_and_test(conform)
add_src_and_test(embedded_python)
add_src_and_test(global_store)
add_src_and_test(helper)
add_src_and_test(json_store)
add_src_and_test(log)
add_src_and_test(playhead)
add_src_and_test(session)
add_src_and_test(studio)
add_src_and_test(viewport)
