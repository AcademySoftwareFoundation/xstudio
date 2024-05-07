
set(prefs)

macro(add_pref name)
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/bin/preference/${name}
                   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${name}
                                                    ${CMAKE_BINARY_DIR}/bin/preference/${name}
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${name})
  list(APPEND prefs ${CMAKE_BINARY_DIR}/bin/preference/${name})
endmacro()

file(GLOB PREFFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.json)

foreach(PREFFile ${PREFFILES})
	get_filename_component(PREFNAME ${PREFFile} NAME)
	add_pref(${PREFNAME})
endforeach()

add_custom_target(
	preferences-copy ALL DEPENDS
  ${prefs}
)

if(WIN32)
	install(FILES
	${prefs}
	DESTINATION
	${CMAKE_INSTALL_PREFIX}/preference)
else()
	install(FILES
	  ${prefs}
	  DESTINATION share/xstudio/preference)
endif()