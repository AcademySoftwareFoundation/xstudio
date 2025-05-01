find_package(OpenSSL)


SET(LINK_DEPS
	xstudio::utility
	xstudio::json_store
	CAF::core
	OpenSSL::SSL
)

create_component(scanner ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")
