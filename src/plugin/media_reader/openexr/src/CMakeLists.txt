find_package(OpenEXR)
find_package(Imath)

SET(LINK_DEPS
	xstudio::media_reader
	OpenEXR::OpenEXR
	Imath::Imath
)

create_plugin_with_alias(media_reader_openexr xstudio::media_reader::openexr 0.1.0  "${LINK_DEPS}")
