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
	CAF::core
	Imath::Imath
	fmt::fmt
	OTIO::opentime
	OTIO::opentimelineio
)

SET(STATIC_LINK_DEPS
	xstudio::utility_static
	xstudio::media_static
	CAF::core
	Imath::Imath
	fmt::fmt
	OTIO::opentime
	OTIO::opentimelineio
)

create_component_static(timeline ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}" "${STATIC_LINK_DEPS}")

