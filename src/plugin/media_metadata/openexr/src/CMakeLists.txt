find_package(OpenEXR)
find_package(Imath)

SET(LINK_DEPS
	xstudio::media_metadata
	OpenEXR::OpenEXR
	Imath::Imath
)

create_plugin_with_alias(media_metadata_openexr xstudio::media_metadata::openexr 0.1.0  "${LINK_DEPS}")
