include(CTest)

SET(LINK_DEPS
	xstudio::http_client
)

create_tests("${LINK_DEPS}")
