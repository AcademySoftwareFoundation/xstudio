find_package(OpenColorIO CONFIG)

SET(LINK_DEPS
	xstudio::media_hook
	xstudio::utility
	xstudio::module
	OpenColorIO::OpenColorIO
)

create_plugin_with_alias(media_hook_dneg xstudio::media_hook::dnhook ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")

add_preference(plugin_dneg_media_hook.json ${CMAKE_CURRENT_SOURCE_DIR} media_hook_dneg)