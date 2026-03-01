add_src_and_test(ffmpeg)
add_src_and_test(openexr)
add_src_and_test(ppm)
add_src_and_test(blank)

if (${OPENIMAGEIO_PLUGIN})
    add_src_and_test(openimageio)
endif()

build_studio_plugins("${STUDIO_PLUGINS}")
