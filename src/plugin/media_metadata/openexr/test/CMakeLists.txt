include(CTest)

SET(LINK_DEPS
	caf::core
)

create_tests("${LINK_DEPS}")
