project(viewport_widget VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

QT6_WRAP_CPP(VIEWPORT_OPENGL_UI_MOC_SRC
	"${ROOT_DIR}/include/xstudio/ui/qt/viewport_widget.hpp"
)
QT6_WRAP_CPP(offscreen_viewport_MOC_SRC
	"${ROOT_DIR}/include/xstudio/ui/qt/offscreen_viewport.hpp"
)

find_package(Qt6 COMPONENTS Core Quick Gui OpenGL OpenGLWidgets Concurrent REQUIRED)

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
		CAF::core
		Qt6::OpenGLWidgets
		Qt6::OpenGL
		Qt6::Core
		Qt6::Qml
		Qt6::Quick
		xstudio::ui::opengl::viewport
		xstudio::ui::viewport
		xstudio::thumbnail
		xstudio::ui::qml::helper
)