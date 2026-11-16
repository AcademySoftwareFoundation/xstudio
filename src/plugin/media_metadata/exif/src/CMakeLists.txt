find_package(exiv2 REQUIRED CONFIG NAMES exiv2)

SET(LINK_DEPS
	xstudio::media_metadata
	Exiv2::exiv2lib
)

create_plugin_with_alias(media_metadata_exif xstudio::media_metadata::exif ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")
