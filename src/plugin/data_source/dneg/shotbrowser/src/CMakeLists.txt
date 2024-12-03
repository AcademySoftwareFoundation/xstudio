project(shotbrowser VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 COMPONENTS Concurrent Core Quick Gui Qml Widgets OpenGL REQUIRED)

QT5_WRAP_CPP(SHOTBROWSER_UI_MOC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/shotbrowser_engine_ui.hpp")
QT5_WRAP_CPP(RESULT_MODEL_UI_MOC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/result_model_ui.hpp")
QT5_WRAP_CPP(MODEL_UI_MOC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/model_ui.hpp")
QT5_WRAP_CPP(PRESET_MODEL_UI_MOC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/preset_model_ui.hpp")

set(SOURCES
	shotbrowser_plugin.cpp
	shotbrowser_engine_ui.cpp
	shotbrowser_engine_request_ui.cpp
	shotbrowser_engine_query_ui.cpp
	preset_model_ui.cpp
	model_ui.cpp
	result_model_ui.cpp
	async_request.cpp
	worker.cpp
	query_engine.cpp
	action.cpp
	get_actions.cpp
	post_actions.cpp
	put_actions.cpp
	${SHOTBROWSER_UI_MOC_SRC}
	${MODEL_UI_MOC_SRC}
	${PRESET_MODEL_UI_MOC_SRC}
	${RESULT_MODEL_UI_MOC_SRC}
)

qt5_add_resources(SOURCES qml/ShotBrowser.1/shotbrowser.qrc)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(xstudio::data_source::shotbrowser ALIAS ${PROJECT_NAME})
default_plugin_options(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::module
		xstudio::plugin_manager
		xstudio::ui::qml::helper
		xstudio::shotgun_client
		Qt5::Widgets
		Qt5::Core
		Qt5::Qml
		Qt5::Quick
		Qt5::Concurrent
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

add_subdirectory(qml)

# add_custom_target(COPY_SO)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                 COMMAND ${CMAKE_COMMAND} -E
                     copy ${CMAKE_BINARY_DIR}/bin/plugin/libshotbrowser.so ${CMAKE_BINARY_DIR}/bin/plugin/qml/ShotBrowser.1/)


# add_dependencies(${PROJECT_NAME} COPY_SO)
