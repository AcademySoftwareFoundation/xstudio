include(CTest)

SET(LINK_DEPS
	CAF::core
)

create_tests("${LINK_DEPS}")


