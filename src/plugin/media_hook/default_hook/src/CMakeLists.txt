find_package(OpenColorIO CONFIG)

SET(LINK_DEPS
	xstudio::media_hook
	xstudio::utility
	xstudio::module
	OpenColorIO::OpenColorIO
)

create_plugin_with_alias(default_hook xstudio::media_hook::default_hook ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")
