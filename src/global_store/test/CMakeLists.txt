include(CTest)

SET(LINK_DEPS
	xstudio::global_store
)

create_tests("${LINK_DEPS}")
