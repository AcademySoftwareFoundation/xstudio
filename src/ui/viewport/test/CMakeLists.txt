include(CTest)

SET(LINK_DEPS
	xstudio::ui::viewport
)

create_tests("${LINK_DEPS}")
