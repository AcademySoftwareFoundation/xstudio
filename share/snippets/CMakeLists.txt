
set(snippets)

macro(add_snip name)
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/bin/snippets/${name}
                   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${name}
                                                    ${CMAKE_BINARY_DIR}/bin/snippets/${name}
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${name})
  list(APPEND snippets ${CMAKE_BINARY_DIR}/bin/snippets/${name})
endmacro()

add_snip(Demo/print_status.py)
add_snip(Demo/hello_world.py)
add_snip(Demo/clear_thumbnails.py)
add_snip(Demo/sort_playlists.py)
add_snip(Demo/dump_shots.py)
add_snip(Demo/capitalise_playlists.py)
add_snip(Demo/render_all_annotation.py)
add_snip(Demo/demo_mask_plugin.py)
add_snip(Demo/clear_sequence_colour.py)
add_snip(Demo/flag_clip_step.py)
add_snip(DNeg/export_flagged_clip_shots.py)

add_custom_target(
	snippets-copy ALL DEPENDS
  ${snippets}
)
if(WIN32)
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION ${CMAKE_INSTALL_PREFIX}
    FILES_MATCHING PATTERN "*.py"
  )
else()
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION share/xstudio
    FILES_MATCHING PATTERN "*.py"
  )
endif()
