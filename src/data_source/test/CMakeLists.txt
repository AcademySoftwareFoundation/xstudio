include(CTest)

SET(LINK_DEPS
	xstudio::data_source
	caf::core
)

create_tests("${LINK_DEPS}")


