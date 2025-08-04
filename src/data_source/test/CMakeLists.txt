include(CTest)

SET(LINK_DEPS
	xstudio::data_source
	CAF::core
)

create_tests("${LINK_DEPS}")


