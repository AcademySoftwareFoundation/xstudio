SET(LINK_DEPS
	xstudio::module
	xstudio::plugin_manager
	xstudio::ui::opengl::viewport
	xstudio::ui::viewport
	Imath::Imath
)

find_package(Imath)

create_plugin_with_alias(pixel_probe_hud xstudio::viewport::pixel_probe_hud ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")

add_plugin_qml(${PROJECT_NAME} qml)