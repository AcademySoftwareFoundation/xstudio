SET(LINK_DEPS
	xstudio::utility
)

create_plugin_with_alias(dnrun xstudio::plugin::dnrun 0.1.0  "${LINK_DEPS}")

