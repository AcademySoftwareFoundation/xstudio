

file(GLOB PREFFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.json)

# The web browser prefs only make sense in a WebEngine build; leaving them out
# keeps the "Web Browser" tab out of the preferences dialog otherwise.
if(NOT BUILD_WEBENGINE)
	list(FILTER PREFFILES EXCLUDE REGEX "ui_web_browser\\.json$")
endif()

add_custom_target(PREFERENCES ALL)

foreach(PREFFile ${PREFFILES})
	get_filename_component(PREFNAME ${PREFFile} NAME)
	add_preference(${PREFNAME} ${CMAKE_CURRENT_SOURCE_DIR} PREFERENCES)
endforeach()

