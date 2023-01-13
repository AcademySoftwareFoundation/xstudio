include(CTest)

SET(LINK_DEPS
	xstudio::json_store
)

create_tests("${LINK_DEPS}")

