include(CTest)

SET(LINK_DEPS
	xstudio::media_cache
	CAF::core
)

create_tests("${LINK_DEPS}")
