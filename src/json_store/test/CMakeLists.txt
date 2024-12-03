include(CTest)

SET(LINK_DEPS
	xstudio::json_store
	xstudio::media
)

create_tests("${LINK_DEPS}")

