
set(snippets)

if(APPLE)
  set(_snippets_install_dir "${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/Resources/snippets")
else()
  set(_snippets_install_dir "${CMAKE_BINARY_DIR}/bin/snippets")
endif()

macro(add_snip name)
  add_custom_command(OUTPUT ${_snippets_install_dir}/${name}
                   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${name}
                                                    ${_snippets_install_dir}/${name}
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${name})
  list(APPEND snippets ${_snippets_install_dir}/${name})
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

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DESTINATION share/xstudio
  FILES_MATCHING PATTERN "*.py"
)
