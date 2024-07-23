set(fonts)

macro(add_font name)
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/bin/fonts/${name}
                   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${name}
                                                    ${CMAKE_BINARY_DIR}/bin/fonts/${name}
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${name})
  list(APPEND fonts ${CMAKE_BINARY_DIR}/bin/fonts/${name})
endmacro()

file(GLOB FONTFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.ttf)

foreach(FONTFile ${FONTFILES})
	get_filename_component(FONTNAME ${FONTFile} NAME)
	add_font(${FONTNAME})
endforeach()

add_custom_target(
	fonts-copy ALL DEPENDS
  ${fonts}
)

if(WIN32)
	install(FILES
	${fonts}
	DESTINATION
	${CMAKE_INSTALL_PREFIX}/fonts)
else()
	install(FILES
	  ${fonts}
	  DESTINATION share/xstudio/fonts)
endif()