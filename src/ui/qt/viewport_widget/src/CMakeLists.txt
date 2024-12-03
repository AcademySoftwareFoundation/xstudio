project(viewport_widget VERSION 0.1.0 LANGUAGES CXX)

QT5_WRAP_CPP(VIEWPORT_OPENGL_UI_MOC_SRC
	"${ROOT_DIR}/include/xstudio/ui/qt/viewport_widget.hpp"
)
QT5_WRAP_CPP(offscreen_viewport_MOC_SRC
	"${ROOT_DIR}/include/xstudio/ui/qt/offscreen_viewport.hpp"
)

# find_package(OpenGL REQUIRED)
# find_package(GLEW REQUIRED)

set(SOURCES
	viewport_widget.cpp
	offscreen_viewport.cpp
	${VIEWPORT_OPENGL_UI_MOC_SRC}
	${offscreen_viewport_MOC_SRC}
)

# TODO: where should this go!?

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(
	xstudio::ui::qt::viewport_widget ALIAS ${PROJECT_NAME}
)

default_options_qt(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		${CAF_LIBRARY_core}
		Qt5::Widgets
		Qt5::OpenGL
		Qt5::Core
		Qt5::Qml
		Qt5::Quick
		xstudio::ui::opengl::viewport
		xstudio::ui::viewport
		xstudio::thumbnail
		xstudio::ui::qml::helper
)