project(session_snapshots VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

find_package(Qt6 COMPONENTS Core Quick Gui Widgets Concurrent REQUIRED)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

QT6_WRAP_CPP(SNAPSHOTS_MODEL_UI_MOC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/snapshot_model_ui.hpp")

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
		Qt6::Core
		Qt6::Quick
		Qt6::Gui
		Qt6::Widgets
		Qt6::Concurrent
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

add_plugin_qml(${PROJECT_NAME} qml)
