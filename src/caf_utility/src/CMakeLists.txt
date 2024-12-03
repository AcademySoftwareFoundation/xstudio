find_package(fmt REQUIRED)

SET(LINK_DEPS
	caf::io
	caf::core
	fmt::fmt
)

create_component(caf_utility 0.1.0 "${LINK_DEPS}")