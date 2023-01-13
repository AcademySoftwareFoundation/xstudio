SET(LINK_DEPS
	xstudio::utility
)

create_plugin_with_alias(disable_screensaver xstudio::plugin::disable_screensaver 0.1.0  "${LINK_DEPS}")
