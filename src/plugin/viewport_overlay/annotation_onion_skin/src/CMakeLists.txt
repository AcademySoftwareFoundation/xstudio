
SET(LINK_DEPS
	xstudio::module
	xstudio::plugin_manager
	xstudio::ui::opengl::viewport
	Imath::Imath
)

find_package(Imath)

create_plugin_with_alias(
	annotation_onion_skin
	xstudio::viewport::annotation_onion_skin
	${XSTUDIO_GLOBAL_VERSION}
	"${LINK_DEPS}")
