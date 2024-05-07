
set(snippets)

macro(add_snip name)
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/bin/snippets/${name}
                   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${name}
                                                    ${CMAKE_BINARY_DIR}/bin/snippets/${name}
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${name})
  list(APPEND snippets ${CMAKE_BINARY_DIR}/bin/snippets/${name})
endmacro()

add_snip(demo.json)

add_custom_target(
	snippets-copy ALL DEPENDS
  ${snippets}
)

if(WIN32)
	install(FILES
	  ${snippets}
	  DESTINATION
	  ${CMAKE_INSTALL_PREFIX}/snippets)
else()
	install(FILES
	  ${snippets}
	  DESTINATION share/xstudio/snippets)
endif()
