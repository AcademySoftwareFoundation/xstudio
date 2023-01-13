include(CTest)

SET(LINK_DEPS
	xstudio::ui::viewport
	xstudio::ui::opengl::viewport
	Imath::Imath
)

find_package(Imath)

create_tests("${LINK_DEPS}")

