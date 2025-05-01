list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules")

find_package(Python COMPONENTS Interpreter)
find_package(Doxygen REQUIRED)
find_package(Sphinx REQUIRED)

# Find all the public headers
set(XSTUDIO_PUBLIC_HEADERS)
set(DOC_TARGETS
	xstudio::utility
	xstudio::json_store
)

set(PYTHONVP "python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}")

foreach(target_var ${DOC_TARGETS})
	get_target_property(_PUBLIC_HEADER_DIRS ${target_var} INTERFACE_INCLUDE_DIRECTORIES)
	foreach(include_dir ${_PUBLIC_HEADER_DIRS})
		string(REGEX REPLACE "\\$<BUILD_INTERFACE:" "" include_dir "${include_dir}")
		string(REGEX REPLACE ">" "" include_dir "${include_dir}")
		file(GLOB_RECURSE _PUBLIC_HEADERS ${include_dir}/*.hpp)
		list(APPEND XSTUDIO_PUBLIC_HEADERS ${_PUBLIC_HEADERS})
	endforeach(include_dir)
endforeach(target_var)

set(DOXYGEN_INPUT_DIR ${PROJECT_SOURCE_DIR})
set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs/doxygen)
set(DOXYGEN_INDEX_FILE ${DOXYGEN_OUTPUT_DIR}/xml/index.xml)
set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
set(CONFFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/conf.py.in)
set(CONFFILE_OUT ${CMAKE_CURRENT_SOURCE_DIR}/conf.py)

#Replace variables inside @@ with the current values
configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)
configure_file(${CONFFILE_IN} ${CONFFILE_OUT} @ONLY)

file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR}) #Doxygen won't create this for us
file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR}/html) #Doxygen won't create this for us
file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR}/latex) #Doxygen won't create this for us
file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR}/xml) #Doxygen won't create this for us


add_custom_command(OUTPUT ${DOXYGEN_INDEX_FILE}
					# shut it up...
                    # DEPENDS ${XSTUDIO_PUBLIC_HEADERS} xstudio::python_module
                   COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
                   MAIN_DEPENDENCY ${DOXYFILE_OUT} ${DOXYFILE_IN}
                   COMMENT "Generating docs"
                   VERBATIM)

add_custom_target(Doxygen ALL DEPENDS ${DOXYGEN_INDEX_FILE})

# add_dependencies(Doxygen python_module)

set(SPHINX_SOURCE ${CMAKE_CURRENT_SOURCE_DIR})
set(SPHINX_BUILD ${CMAKE_CURRENT_BINARY_DIR}/docs/sphinx)
set(SPHINX_INDEX_FILE ${SPHINX_BUILD}/index.html)

                  # ${SPHINX_EXECUTABLE} -q -b html
add_custom_target(Sphinx ALL
				  DEPENDS ${DOXYGEN_INDEX_FILE}
                  COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/../bin/python/lib/${PYTHONVP}/site-packages:$ENV{PYTHONPATH}"
                  ${SPHINX_EXECUTABLE} -b html
                  -D breathe_projects.xStudio=${DOXYGEN_OUTPUT_DIR}/xml
                  -D version=${PROJECT_VERSION}
                  -D release=${PROJECT_VERSION}
                  ${SPHINX_SOURCE} ${SPHINX_BUILD}
                  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                  COMMENT "Generating documentation with Sphinx")

set(USER_DOCS_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/user_docs)
set(USER_DOCS_BUILD ${CMAKE_CURRENT_BINARY_DIR}/../bin/user_docs)

add_custom_target(UserDocs ALL
				  COMMAND ${SPHINX_EXECUTABLE} -q
				  -D version=${PROJECT_VERSION}
				  -D release=${PROJECT_VERSION}
				  ${USER_DOCS_SOURCE} ${USER_DOCS_BUILD}
				  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                  COMMENT "Generating user documentation with Sphinx")

if (APPLE)
install(DIRECTORY ${SPHINX_BUILD}/ DESTINATION ${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/Resources/docs)
else()
install(DIRECTORY ${SPHINX_BUILD}/ DESTINATION share/xstudio/docs)
endif()

# https://devblogs.microsoft.com/cppblog/clear-functional-c-documentation-with-sphinx-breathe-doxygen-cmake/
