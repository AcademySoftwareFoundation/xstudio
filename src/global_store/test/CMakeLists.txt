include(CTest)

SET(LINK_DEPS
	xstudio::global_store
	xstudio::media
)

create_tests("${LINK_DEPS}")
