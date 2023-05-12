
SET(LINK_DEPS
	xstudio::module
	xstudio::plugin_manager
	xstudio::ui::opengl::viewport
	Imath::Imath
)

find_package(Imath)

create_plugin_with_alias(basic_viewport_masking xstudio::viewport::basic_viewport_masking 0.1.0  "${LINK_DEPS}")

add_subdirectory(qml)