add_src_and_test(blank)
add_src_and_test(ffmpeg)
add_src_and_test(openexr)
add_src_and_test(ppm)

find_package(Qt6 OPTIONAL_COMPONENTS Pdf)

IF(TARGET Qt6::Pdf)
	add_src_and_test(pdf)
endif()

if (${OPENIMAGEIO_PLUGIN})
    add_src_and_test(openimageio)
endif()

build_studio_plugins("${STUDIO_PLUGINS}")
