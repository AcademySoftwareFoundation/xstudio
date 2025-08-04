SET(LINK_DEPS
	xstudio::module
	xstudio::plugin_manager
	xstudio::ui::opengl::viewport
	xstudio::ui::viewport
	Imath::Imath
)

find_package(Imath)

create_plugin_with_alias(exr_data_window xstudio::viewport::exr_data_window ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")