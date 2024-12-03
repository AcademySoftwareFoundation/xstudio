project(session_snapshots VERSION 0.1.0 LANGUAGES CXX)

find_package(Qt5 COMPONENTS Core Quick Gui Widgets Concurrent REQUIRED)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

QT5_WRAP_CPP(SNAPSHOTS_MODEL_UI_MOC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/snapshot_model_ui.hpp")

set(SOURCES
	snapshot_model_ui.cpp
	${SNAPSHOTS_MODEL_UI_MOC_SRC}
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(xstudio::utility::session_snapshots ALIAS ${PROJECT_NAME})
default_plugin_options(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::utility
		xstudio::plugin_manager
		xstudio::ui::qml::helper
		xstudio::plugin_manager
		Qt5::Core
		Qt5::Quick
		Qt5::Gui
		Qt5::Widgets
		Qt5::Concurrent
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

add_subdirectory(qml)

IF(UNIX)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                 COMMAND ${CMAKE_COMMAND} -E
                     copy ${CMAKE_BINARY_DIR}/bin/plugin/libsession_snapshots.so ${CMAKE_BINARY_DIR}/bin/plugin/qml/SessionSnapshots.1/)
ELSEIF(WIN32)
ENDIF()
