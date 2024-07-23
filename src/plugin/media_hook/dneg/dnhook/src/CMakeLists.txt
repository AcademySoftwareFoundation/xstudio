find_package(OpenColorIO CONFIG)

SET(LINK_DEPS
	xstudio::media_hook
	xstudio::utility
	OpenColorIO::OpenColorIO
)

create_plugin_with_alias(media_hook_dneg xstudio::media_hook::dnhook 0.1.0  "${LINK_DEPS}")
