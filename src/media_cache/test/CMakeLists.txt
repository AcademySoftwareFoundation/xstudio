include(CTest)

SET(LINK_DEPS
	xstudio::media_cache
	caf::core
)

create_tests("${LINK_DEPS}")
