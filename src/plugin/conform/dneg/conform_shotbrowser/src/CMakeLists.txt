SET(LINK_DEPS
	xstudio::conform
	xstudio::utility
)

create_plugin_with_alias(conform_shotbrowser xstudio::conform::shotbrowser ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")
