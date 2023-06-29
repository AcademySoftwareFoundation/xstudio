find_package(OpenSSL)


SET(LINK_DEPS
	xstudio::utility
	xstudio::json_store
	caf::core
	OpenSSL::SSL
)

create_component(scanner 0.1.0 "${LINK_DEPS}")
