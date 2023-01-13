SET(LINK_DEPS
	caf::core
	JPEG::JPEG
	xstudio::global_store
)

find_package(JPEG REQUIRED)

create_component(thumbnail 0.1.0 "${LINK_DEPS}")
