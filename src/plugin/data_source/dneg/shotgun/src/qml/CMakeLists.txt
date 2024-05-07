
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 COMPONENTS Qml Core Quick Gui Widgets OpenGL REQUIRED)
configure_file(.clang-tidy .clang-tidy)

# find_package(Qt5 COMPONENTS Core Gui Widgets OpenGL QUIET)

# QT5_ADD_RESOURCES(PROTOTYPE_RCS)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpic")
	set(DCMAKE_EXE_LINKER_FLAGS "${DCMAKE_EXE_LINKER_FLAGS} -fpic")
endif

project(data_source_shotgun_ui VERSION 0.1.0 LANGUAGES CXX)

QT5_WRAP_CPP(DATA_SOURCE_SHOTGUN_UI_MOC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/data_source_shotgun_ui.hpp")
QT5_WRAP_CPP(SHOTGUN_MODEL_UI_MOC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/shotgun_model_ui.hpp")

set(SOURCES
	shotgun_model_ui.cpp
	data_source_shotgun_ui.cpp
	data_source_shotgun_requests_ui.cpp
	data_source_shotgun_query_ui.cpp
	../data_source_shotgun_query_engine.cpp
	${DATA_SOURCE_SHOTGUN_UI_MOC_SRC}
	${SHOTGUN_MODEL_UI_MOC_SRC}
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
default_options_qt(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::ui::qml::helper
		xstudio::shotgun_client
		${CAF_LIBRARY_core}
		Qt5::Core
		Qt5::Qml
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugin/qml/Shotgun.1"
)

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION share/xstudio/plugin/qml/Shotgun.1)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Shotgun.1/ DESTINATION share/xstudio/plugin/qml/Shotgun.1)

file(GLOB QMLFILES  ${CMAKE_CURRENT_SOURCE_DIR}/Shotgun.1/*)

add_custom_target(COPY_QML)

foreach(QMLFile ${QMLFILES})
  add_custom_command(TARGET COPY_QML POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E
                         copy ${QMLFile} ${CMAKE_BINARY_DIR}/bin/plugin/qml/Shotgun.1/)
endforeach()


add_dependencies(${PROJECT_NAME} COPY_QML)
