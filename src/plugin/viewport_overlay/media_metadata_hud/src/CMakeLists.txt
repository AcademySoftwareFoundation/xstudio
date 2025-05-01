
SET(LINK_DEPS
	xstudio::module
	xstudio::plugin_manager
	xstudio::ui::opengl::viewport
	Imath::Imath
)

find_package(Imath)

create_plugin_with_alias(media_metadata_hud xstudio::viewport::media_metadata_hud ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")

add_plugin_qml(${PROJECT_NAME} qml)