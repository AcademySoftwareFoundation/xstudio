

add_custom_target(BuiltDocs ALL
				  COMMAND cp -r 
				  ${CMAKE_CURRENT_SOURCE_DIR}
				  ${CMAKE_BINARY_DIR}/bin
				  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
          COMMENT "Copying built docs to build folder")

install(DIRECTORY
  ${CMAKE_CURRENT_SOURCE_DIR}/
  DESTINATION share/xstudio/docs)
