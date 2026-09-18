include(CTest)

SET(LINK_DEPS
	xstudio::shotgun_client
	xstudio::media
	xstudio::utility
	CAF::core
)

create_tests("${LINK_DEPS}")
