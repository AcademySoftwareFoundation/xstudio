SET(LINK_DEPS
	xstudio::conform
	xstudio::utility
)

create_plugin_with_alias(conform_shotgun xstudio::conform::shotgun 0.1.0  "${LINK_DEPS}")
