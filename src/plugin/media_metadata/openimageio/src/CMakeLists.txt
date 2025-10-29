find_package(OpenImageIO)

SET(LINK_DEPS
	xstudio::media_metadata
	OpenImageIO::OpenImageIO
)

create_plugin_with_alias(media_metadata_openimageio xstudio::media_metadata::openimageio ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")
