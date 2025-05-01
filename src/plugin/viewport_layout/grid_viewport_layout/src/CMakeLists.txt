
SET(LINK_DEPS
	xstudio::module
	xstudio::plugin_manager
	xstudio::ui::opengl::viewport
	Imath::Imath
)

find_package(Imath)

create_plugin_with_alias(grid_viewport_layout xstudio::viewport::grid_viewport_layout ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")