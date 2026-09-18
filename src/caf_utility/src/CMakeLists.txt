find_package(fmt REQUIRED)

SET(LINK_DEPS
	CAF::io
	CAF::core
	fmt::fmt
)

create_component(caf_utility ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")