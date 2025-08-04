SET(LINK_DEPS
	xstudio::media
	xstudio::utility
	xstudio::subset
	CAF::core
)

create_component(contact_sheet ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")
