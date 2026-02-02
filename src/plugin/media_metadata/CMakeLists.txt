add_src_and_test(ffprobe)
add_src_and_test(openexr)
if (${OPENIMAGEIO_PLUGIN})
    add_src_and_test(openimageio)
endif()

build_studio_plugins("${STUDIO_PLUGINS}")
