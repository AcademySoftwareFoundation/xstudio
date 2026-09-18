if (NOT WIN32)
	SET(LINK_DEPS
		xstudio::utility
	)

	create_plugin_with_alias(dnrun xstudio::plugin::dnrun ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")
endif()