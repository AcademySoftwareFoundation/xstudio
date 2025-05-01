find_package(OpenColorIO CONFIG)

SET(LINK_DEPS
	xstudio::media_hook
	xstudio::utility
	xstudio::module
	OpenColorIO::OpenColorIO
)

create_plugin_with_alias(media_hook_dneg xstudio::media_hook::dnhook ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")
