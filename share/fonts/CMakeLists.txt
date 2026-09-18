file(GLOB FONTFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.ttf)

add_custom_target(FONTS ALL)

foreach(FONTFile ${FONTFILES})
	get_filename_component(FONTNAME ${FONTFile} NAME)
	add_font(${FONTNAME} ${CMAKE_CURRENT_SOURCE_DIR} FONTS)
endforeach()