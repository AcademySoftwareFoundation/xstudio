SET(LINK_DEPS
	xstudio::utility
	xstudio::http_client
	CAF::core
)

find_package(OpenSSL)
find_package(ZLIB)

create_component(shotgun_client ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")
