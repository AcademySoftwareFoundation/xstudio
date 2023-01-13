SET(LINK_DEPS
	xstudio::media
	xstudio::utility
	xstudio::broadcast
	caf::core
)

create_component(contact_sheet 0.1.0 "${LINK_DEPS}")
