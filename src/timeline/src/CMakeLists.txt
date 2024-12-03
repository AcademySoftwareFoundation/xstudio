find_package(Imath REQUIRED)
find_package(fmt REQUIRED)
if(NOT ${OTIO_SUBMODULE})
	find_package(OpenTime REQUIRED)
	find_package(OpenTimelineIO REQUIRED)
endif()

SET(LINK_DEPS
	xstudio::utility
	xstudio::media
	xstudio::playhead
	caf::core
	Imath::Imath
	fmt::fmt
	OTIO::opentime
	OTIO::opentimelineio
)

SET(STATIC_LINK_DEPS
	xstudio::utility_static
	xstudio::media_static
	caf::core
	Imath::Imath
	fmt::fmt
	OTIO::opentime
	OTIO::opentimelineio
)

create_component_static(timeline 0.1.0 "${LINK_DEPS}" "${STATIC_LINK_DEPS}")

