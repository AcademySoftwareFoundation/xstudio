find_package(OpenImageIO REQUIRED)

SET(LINK_DEPS
	xstudio::media_reader
	OpenImageIO::OpenImageIO
)

create_plugin_with_alias(
	media_reader_oiio 
	xstudio::media_reader::oiio 
	${XSTUDIO_GLOBAL_VERSION} 
	"${LINK_DEPS}"
)

target_include_directories(media_reader_oiio PRIVATE ${OPENIMAGEIO_INCLUDE_DIR})

