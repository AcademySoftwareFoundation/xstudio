SET(LINK_DEPS
	xstudio::module
	xstudio::plugin_manager
	xstudio::ui::opengl::viewport
	xstudio::ui::viewport
	Imath::Imath
)

find_package(Imath)

create_plugin_with_alias(image_boundary_hud xstudio::viewport::image_boundary_hud 0.1.0  "${LINK_DEPS}")