

file(GLOB PREFFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.json)

add_custom_target(PREFERENCES ALL)

foreach(PREFFile ${PREFFILES})
	get_filename_component(PREFNAME ${PREFFile} NAME)
	add_preference(${PREFNAME} ${CMAKE_CURRENT_SOURCE_DIR} PREFERENCES)
endforeach()

