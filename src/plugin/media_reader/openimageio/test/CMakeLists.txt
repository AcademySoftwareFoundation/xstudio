SET(LINK_DEPS
	xstudio::media_reader
	OpenImageIO::OpenImageIO
)

create_tests("${LINK_DEPS}")

