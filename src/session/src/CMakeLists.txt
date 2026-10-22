find_package(ZLIB REQUIRED)

SET(LINK_DEPS
	xstudio::playlist
	xstudio::utility
	xstudio::bookmark
	CAF::core
)

create_component(session ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")

# session_actor.cpp uses zstr (zlib) directly. utility no longer exports
# ZLIB::ZLIB publicly, so link it here as a private implementation detail.
target_link_libraries(session PRIVATE ZLIB::ZLIB)
