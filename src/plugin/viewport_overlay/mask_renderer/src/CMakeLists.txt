
SET(LINK_DEPS
	xstudio::module
	xstudio::plugin_manager
	xstudio::ui::opengl::viewport
	Imath::Imath
)

find_package(Imath)

create_plugin_with_alias(mask_renderer xstudio::viewport::mask_renderer ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")

