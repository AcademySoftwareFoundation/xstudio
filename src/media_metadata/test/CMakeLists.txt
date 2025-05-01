include(CTest)

SET(LINK_DEPS
	xstudio::media_metadata
	xstudio::global
	fmt::fmt
	CAF::core
)

create_tests("${LINK_DEPS}")
