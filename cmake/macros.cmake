
macro(add_preference name path target)
    add_custom_command(TARGET ${target}
                   COMMAND ${CMAKE_COMMAND} -E copy ${path}/${name}
                                                    ${CMAKE_BINARY_DIR}/bin/preference/${name}
                   DEPENDS ${path}/${name})
    if(INSTALL_XSTUDIO)
		if(WIN32)
			install(FILES
			${path}/${name}
			DESTINATION
			${CMAKE_INSTALL_PREFIX}/preference)
		else()
			install(FILES
			${path}/${name}
			DESTINATION share/xstudio/preference)
		endif()
    endif ()

endmacro()

macro(default_compile_options name)
	target_compile_options(${name}
		# PRIVATE -fvisibility=hidden
		PRIVATE $<$<AND:$<CONFIG:RelWithDebInfo>,$<PLATFORM_ID:Linux>>:-fno-omit-frame-pointer>
		PRIVATE $<$<AND:$<CONFIG:RelWithDebInfo>,$<PLATFORM_ID:Windows>>:/Oy>
		PRIVATE $<$<PLATFORM_ID:Linux>:-Wno-deprecated>
		# PRIVATE $<$<CONFIG:Debug>:-Wno-unused-variable>
		# PRIVATE $<$<CONFIG:Debug>:-Wno-unused-but-set-variable>
		# PRIVATE $<$<CONFIG:Debug>:-Wno-unused-parameter>
		PRIVATE $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Linux>>:-Wno-unused-function>
		PRIVATE $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Linux>>:-Wextra>
		PRIVATE $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Linux>>:-Wextra>
		PRIVATE $<$<PLATFORM_ID:Linux>:-Wfatal-errors> # Stop after first error
		PRIVATE $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Linux>>:-Wpedantic>
		PRIVATE $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Windows>>:/wd4100>
		# PRIVATE $<$<CONFIG:Debug>:-Wall>
		# PRIVATE $<$<CONFIG:Debug>:-Werror>
		# PRIVATE $<$<CONFIG:Debug>:-Wextra>
		# PRIVATE $<$<CONFIG:Debug>:-Wpedantic>
		# PRIVATE ${GTEST_CFLAGS}
	)

	target_compile_features(${name}
		PUBLIC cxx_std_20
	)

	target_compile_definitions(${name}
		PUBLIC $<$<BOOL:${BUILD_TESTING}>:test_private=public>
		PUBLIC $<$<NOT:$<BOOL:${BUILD_TESTING}>>:test_private=private>
		PRIVATE -DSPDLOG_FMT_EXTERNAL
		$<$<CXX_COMPILER_ID:GNU>:_GNU_SOURCE> # Define _GNU_SOURCE for Linux
		$<$<PLATFORM_ID:Linux>:__linux__> # Define __linux__ for Linux
		$<$<PLATFORM_ID:Windows>:_WIN32> # Define _WIN32 for Windows
		PRIVATE XSTUDIO_GLOBAL_VERSION=\"${XSTUDIO_GLOBAL_VERSION}\"
		PRIVATE XSTUDIO_GLOBAL_NAME=\"${XSTUDIO_GLOBAL_NAME}\"
		PUBLIC PROJECT_VERSION=\"${PROJECT_VERSION}\"
		PUBLIC BINARY_DIR=\"${CMAKE_BINARY_DIR}/bin\"
		PRIVATE TEST_RESOURCE=\"${TEST_RESOURCE}\"
		PRIVATE ROOT_DIR=\"${ROOT_DIR}\"
		PRIVATE $<$<CONFIG:Debug>:XSTUDIO_DEBUG=1>
		$<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>	
	)
endmacro()

if (BUILD_TESTING)
	macro(default_compile_options_gtest name)
		target_compile_options(${name}
			# PRIVATE -fvisibility=hidden
			PRIVATE $<$<CONFIG:RelWithDebInfo>:-fno-omit-frame-pointer>
			PRIVATE -Wno-deprecated
			# PRIVATE $<$<CONFIG:Debug>:-Wno-unused-variable>
			# PRIVATE $<$<CONFIG:Debug>:-Wno-unused-but-set-variable>
			# PRIVATE $<$<CONFIG:Debug>:-Wno-unused-parameter>
			PRIVATE $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Linux>>:-Wno-unused-function>
			# PRIVATE $<$<CONFIG:Debug>:-Wall>
			# PRIVATE $<$<CONFIG:Debug>:-Werror>
			PRIVATE $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Linux>>:-Wextra>
			PRIVATE $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Linux>>:-Wpedantic>
			PRIVATE ${GTEST_CFLAGS}
		)

		target_compile_features(${name}
			PUBLIC cxx_std_20
		)

		target_compile_definitions(${name}
			PUBLIC $<$<BOOL:${BUILD_TESTING}>:test_private=public>
			PUBLIC $<$<NOT:$<BOOL:${BUILD_TESTING}>>:test_private=private>
			PRIVATE XSTUDIO_GLOBAL_VERSION=\"${XSTUDIO_GLOBAL_VERSION}\"
			PRIVATE XSTUDIO_GLOBAL_NAME=\"${XSTUDIO_GLOBAL_NAME}\"
			PUBLIC PROJECT_VERSION=\"${PROJECT_VERSION}\"
			PRIVATE SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\"
			PUBLIC BINARY_DIR=\"${CMAKE_BINARY_DIR}/bin\"
			PRIVATE TEST_RESOURCE=\"${TEST_RESOURCE}\"
			PRIVATE ROOT_DIR=\"${ROOT_DIR}\"
			PRIVATE $<$<CONFIG:Debug>:XSTUDIO_DEBUG=1>
		)
	endmacro()
endif (BUILD_TESTING)

macro(default_options_local name)
	if (NOT CAF_FOUND)
		find_package(CAF COMPONENTS core io)
	endif (NOT CAF_FOUND)

	find_package(spdlog CONFIG REQUIRED)

	default_compile_options(${name})
	target_include_directories(${name}
	    PUBLIC
	        $<BUILD_INTERFACE:${ROOT_DIR}/include>
	        # $<INSTALL_INTERFACE:include>
	    PRIVATE
	        ${CMAKE_CURRENT_SOURCE_DIR}/src
	    SYSTEM PUBLIC
	    	$<BUILD_INTERFACE:${ROOT_DIR}/extern/include>
	    	$<BUILD_INTERFACE:${ROOT_DIR}/extern/otio/OpenTimelineIO/src>
	)
	set_target_properties(${name}
	    PROPERTIES
	    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/lib"
	)
endmacro()

macro(default_options name)
	default_options_local(${name})
	install(TARGETS ${name} EXPORT xstudio
        LIBRARY DESTINATION share/xstudio/lib)
	target_include_directories(${name} INTERFACE
  		$<BUILD_INTERFACE:${ROOT_DIR}/include>
  		$<BUILD_INTERFACE:${ROOT_DIR}/extern/include>
    	$<BUILD_INTERFACE:${ROOT_DIR}/extern/otio/OpenTimelineIO/src>
  		$<INSTALL_INTERFACE:include>
		$<INSTALL_INTERFACE:extern/include>
		)
endmacro()

macro(default_options_static name)
	if (NOT CAF_FOUND)
		find_package(CAF COMPONENTS core io)
	endif (NOT CAF_FOUND)

	find_package(spdlog CONFIG REQUIRED)

	default_compile_options(${name})
	target_include_directories(${name}
	    PUBLIC
	        $<BUILD_INTERFACE:${ROOT_DIR}/include>
	        # $<INSTALL_INTERFACE:include>
	    PRIVATE
	        ${CMAKE_CURRENT_SOURCE_DIR}/src
	    SYSTEM PUBLIC
	    	$<BUILD_INTERFACE:${ROOT_DIR}/extern/include>
	    	$<BUILD_INTERFACE:${ROOT_DIR}/extern/otio/OpenTimelineIO/src>
	)
	set_target_properties(${name}
	    PROPERTIES
	    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/lib"
	)
endmacro()

macro(default_plugin_options name)
	if (NOT CAF_FOUND)
		find_package(CAF COMPONENTS core io)
	endif (NOT CAF_FOUND)

	find_package(spdlog CONFIG REQUIRED)
	default_compile_options(${name})
	target_include_directories(${name}
	    PUBLIC
	        $<BUILD_INTERFACE:${ROOT_DIR}/include>
	        # $<INSTALL_INTERFACE:include>
	    PRIVATE
	        ${CMAKE_CURRENT_SOURCE_DIR}/src
	    SYSTEM PUBLIC
	    	$<BUILD_INTERFACE:${ROOT_DIR}/extern/include>
	    	$<BUILD_INTERFACE:${ROOT_DIR}/extern/otio/OpenTimelineIO/src>
	)
	set_target_properties(${name}
	    PROPERTIES
	    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugin"
	)
	install(TARGETS ${name}
        LIBRARY DESTINATION share/xstudio/plugin)

	if(WIN32)

		#This will unfortunately also install the plugin in the /bin directory.  TODO: Figure out how to omit the plugin itself.
		install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin )
		# We don't want the vcpkg install because it forces dependences; we just want the plugin.
		_install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/plugin)

		#For interactive debugging, we want only the output dll to be copied to the build plugins folder.
		add_custom_command(
			TARGET ${PROJECT_NAME}
			POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${CMAKE_CURRENT_BINARY_DIR}/plugin"
		)
	endif()

endmacro()

if (BUILD_TESTING)
	macro(default_options_gtest name)
		find_package(PkgConfig)
		pkg_search_module(GTEST REQUIRED gtest_main)
		if (NOT CAF_FOUND)
			find_package(CAF COMPONENTS core io)
		endif (NOT CAF_FOUND)
		find_package(spdlog CONFIG REQUIRED)
		default_compile_options_gtest(${name})
		target_include_directories(${name}
		    PUBLIC
		        $<BUILD_INTERFACE:${ROOT_DIR}/include>
		        # $<INSTALL_INTERFACE:include>
		    PRIVATE
		        ${CMAKE_CURRENT_SOURCE_DIR}/src
		    SYSTEM PUBLIC
		    	$<BUILD_INTERFACE:${ROOT_DIR}/extern/include>
		    	$<BUILD_INTERFACE:${ROOT_DIR}/extern/otio/OpenTimelineIO/src>
		)
	endmacro()
endif (BUILD_TESTING)


macro(default_options_qt name)
	if (NOT CAF_FOUND)
		find_package(CAF COMPONENTS core io)
	endif (NOT CAF_FOUND)
	find_package(spdlog CONFIG REQUIRED)
	default_compile_options(${name})
	target_include_directories(${name}
	    PUBLIC
	        $<BUILD_INTERFACE:${ROOT_DIR}/include>
	        # $<INSTALL_INTERFACE:include>
	    PRIVATE
	        ${CMAKE_CURRENT_SOURCE_DIR}/src
	    SYSTEM PUBLIC
	    	$<BUILD_INTERFACE:${ROOT_DIR}/extern/include>
	        ${Qt5Core_INCLUDE_DIRS}
	        ${Qt5OpenGL_INCLUDE_DIRS}
	        ${Qt5Quick_INCLUDE_DIRS}
	        ${Qt5Gui_INCLUDE_DIRS}
	        ${Qt5Widgets_INCLUDE_DIRS}
	        ${Qt5Concurrent_INCLUDE_DIRS}
	        ${Qt5Qml_INCLUDE_DIRS}
	)
	set_target_properties(${name}
	    PROPERTIES
	    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/lib"
	)
	install(TARGETS ${name} EXPORT xstudio
        LIBRARY DESTINATION share/xstudio/lib)

endmacro()

macro(add_src_and_test NAME)
	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}/CMakeLists.txt)
		add_subdirectory(${NAME})
	endif()

	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}/src)
		add_subdirectory(${NAME}/src)
	endif()

	if (BUILD_TESTING)
		if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}/test)
			add_subdirectory(${NAME}/test)
		endif()
	endif()
endmacro()

macro(add_python_plugin NAME)

	install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${NAME} DESTINATION share/xstudio/plugin-python)

	add_custom_target(COPY_PY_PLUGIN_${NAME} ALL)

	add_custom_command(TARGET COPY_PY_PLUGIN_${NAME} POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E
                         copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/${NAME} ${CMAKE_BINARY_DIR}/bin/plugin-python/${NAME})

endmacro()

macro(create_plugin NAME VERSION DEPS)
	create_plugin_with_alias(${NAME} xstudio::${NAME} ${VERSION} "${DEPS}")
endmacro()

macro(create_plugin_with_alias NAME ALIASNAME VERSION DEPS)

	project(${NAME} VERSION ${VERSION} LANGUAGES CXX)

	file(GLOB SOURCES  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

	add_library(${PROJECT_NAME} SHARED ${SOURCES})
	add_library(${ALIASNAME} ALIAS ${PROJECT_NAME})
	default_plugin_options(${PROJECT_NAME})

	target_link_libraries(${PROJECT_NAME}
		PUBLIC ${DEPS}
	)

	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

endmacro()


macro(create_component NAME VERSION DEPS)
	create_component_with_alias(${NAME} xstudio::${NAME} ${VERSION} "${DEPS}")
endmacro()

macro(create_component_with_alias NAME ALIASNAME VERSION DEPS)

	project(${NAME} VERSION ${VERSION} LANGUAGES CXX)

	file(GLOB SOURCES  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

	add_library(${PROJECT_NAME} SHARED ${SOURCES})
	add_library(${ALIASNAME} ALIAS ${PROJECT_NAME})
	default_options(${PROJECT_NAME})

	target_link_libraries(${PROJECT_NAME}
		PUBLIC ${DEPS}
	)

	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

	if(_WIN32)
	set(CMAKE_CXX_VISIBILITY_PRESET hidden)
	set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
	endif(_WIN32)

	# Generate export header
	include(GenerateExportHeader)
	generate_export_header(${PROJECT_NAME})

endmacro()

macro(create_component_static NAME VERSION DEPS STATICDEPS)
	create_component_static_with_alias(${NAME} xstudio::${NAME} ${VERSION} "${DEPS}" "${STATICDEPS}")
endmacro()

macro(create_component_static_with_alias NAME ALIASNAME VERSION DEPS STATICDEPS)

	project(${NAME} VERSION ${VERSION} LANGUAGES CXX)

	file(GLOB SOURCES  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)


	if(INSTALL_XSTUDIO)
		add_library(${PROJECT_NAME} SHARED ${SOURCES})
		add_library(${ALIASNAME} ALIAS ${PROJECT_NAME})
		default_options(${PROJECT_NAME})

		target_link_libraries(${PROJECT_NAME}
			PUBLIC ${DEPS}
		)
		set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)
	endif(INSTALL_XSTUDIO)

	add_library(${PROJECT_NAME}_static STATIC ${SOURCES})
	add_library(${ALIASNAME}_static ALIAS ${PROJECT_NAME}_static)

	default_options_static(${PROJECT_NAME}_static)

	target_link_libraries(${PROJECT_NAME}_static
		PUBLIC ${STATICDEPS}
	)

endmacro()


macro(create_test PATH DEPS)

	get_filename_component(NAME ${PATH} NAME_WE)
	get_filename_component(FILENAME ${PATH} NAME)
	get_filename_component(PARENT ${PATH} DIRECTORY)
	get_filename_component(PARENT ${PARENT} DIRECTORY)
	get_filename_component(PARENT ${PARENT} NAME)

	add_executable(${NAME} ${FILENAME})
	default_options_gtest(${NAME})
	target_link_libraries(${NAME}
		PRIVATE
		"${DEPS}"
		${GTEST_LDFLAGS}
	)
	set_target_properties(${NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

	add_test(${PARENT}_${NAME} ${NAME})

endmacro()

macro(create_tests DEPS)

	file(GLOB SOURCES  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

	foreach(TEST ${SOURCES})

		create_test(${TEST} "${DEPS}")

	endforeach()

endmacro()

macro(create_qml_component NAME VERSION DEPS EXTRAMOC)
	create_qml_component_with_alias(${NAME} xstudio::ui::qml::${NAME} ${VERSION} "${DEPS}" "${EXTRAMOC}")
endmacro()

macro(create_qml_component_with_alias NAME ALIASNAME VERSION DEPS EXTRAMOC)

	project(${NAME}_qml VERSION ${VERSION} LANGUAGES CXX)

	file(GLOB SOURCES  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

	# add MOCS
	SET(MOCSRC "${EXTRAMOC}")

	if(EXISTS "${ROOT_DIR}/include/xstudio/ui/qml/${NAME}_ui.hpp")
		list(PREPEND MOCSRC "${ROOT_DIR}/include/xstudio/ui/qml/${NAME}_ui.hpp")
	endif()

	SET(SOURCES "${SOURCES}")

	foreach(MOC ${MOCSRC})
		get_filename_component(MOCNAME "${MOC}" NAME_WE)

		QT5_WRAP_CPP(${MOCNAME}_moc ${MOC})

		list(APPEND SOURCES ${${MOCNAME}_moc})
	endforeach()

	add_library(${PROJECT_NAME} SHARED ${SOURCES})
	add_library(${ALIASNAME} ALIAS ${PROJECT_NAME})
	default_options_qt(${PROJECT_NAME})

	# Generate export header
	include(GenerateExportHeader)
	generate_export_header(${PROJECT_NAME} EXPORT_FILE_NAME "${ROOT_DIR}/include/xstudio/ui/qml/${PROJECT_NAME}_export.h")
	target_link_libraries(${PROJECT_NAME}
		PUBLIC ${DEPS}
	)

	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)
	set_property(TARGET ${PROJECT_NAME} PROPERTY AUTOMOC ON)

	## Add the directory containing the generated export header to the include directories
	#target_include_directories(${PROJECT_NAME} 
	#	PUBLIC ${CMAKE_BINARY_DIR}  # Include the build directory
	#)

endmacro()

macro(build_studio_plugins STUDIO)
	if(NOT "${STUDIO}" STREQUAL "")

		if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${STUDIO})

			file(GLOB DIRS ${CMAKE_CURRENT_SOURCE_DIR}/${STUDIO}/*)
			foreach(DIR ${DIRS})
			    if(IS_DIRECTORY ${DIR})
			    	get_filename_component(PLUGINNAME ${DIR} NAME)
					add_src_and_test(${STUDIO}/${PLUGINNAME})
					if(IS_DIRECTORY ${DIR}/share/preference)
						file(GLOB PREFFILES ${DIR}/share/preference/*.json)

						add_custom_target(${STUDIO}_${PLUGINNAME}_PREFERENCES ALL)
						foreach(PREFFile ${PREFFILES})
							get_filename_component(PREFNAME ${PREFFile} NAME)
							add_preference(${PREFNAME} ${DIR}/share/preference ${STUDIO}_${PLUGINNAME}_PREFERENCES)
						endforeach()

					endif ()
				endif()
			endforeach()

		endif()

	endif()

endmacro()


macro(set_python_to_proper_build_type)
	#TODO Resolve linking error when running debug build: https://github.com/pybind/pybind11/issues/3403
endmacro()


