SET(LINK_DEPS
	xstudio::module
	xstudio::plugin_manager
	xstudio::ui::opengl::viewport
	xstudio::ui::viewport
	Imath::Imath
)

find_package(Imath)

create_plugin_with_alias(pixel_probe_hud xstudio::viewport::pixel_probe_hud 0.1.0  "${LINK_DEPS}")

add_subdirectory(qml)