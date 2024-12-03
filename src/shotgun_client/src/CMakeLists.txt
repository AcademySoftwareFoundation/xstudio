SET(LINK_DEPS
	xstudio::utility
	xstudio::http_client
	caf::core
)

find_package(OpenSSL)
find_package(ZLIB)

create_component(shotgun_client 0.1.0 "${LINK_DEPS}")
