macro(default_compile_options name)
	target_compile_options(${name}
		# PRIVATE -fvisibility=hidden
		PRIVATE $<$<AND:$<CONFIG:RelWithDebInfo>,$<PLATFORM_ID:Linux>>:-fno-omit-frame-pointer>
		PRIVATE $<$<AND:$<CONFIG:RelWithDebInfo>,$<PLATFORM_ID:Windows>>:/Oy>
		PRIVATE $<$<PLATFORM_ID:Darwin>:-Wno-deprecated>
		PRIVATE $<$<PLATFORM_ID:Linux>:-Wno-deprecated>
		# PRIVATE $<$<PLATFORM_ID:Linux>:-Wno-deprecated-declarations>
		# PRIVATE $<$<CONFIG:Debug>:-Wno-unused-variable>
		# PRIVATE $<$<CONFIG:Debug>:-Wno-unused-but-set-variable>
		# PRIVATE $<$<CONFIG:Debug>:-Wno-unused-parameter>
		PRIVATE $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Linux>>:-Wno-unused-function>
		PRIVATE $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Linux>>:-Wextra>
		PRIVATE $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Linux>>:-Wextra>
		PRIVATE $<$<PLATFORM_ID:Linux>:-Wfatal-errors> # Stop after first error
		PRIVATE $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Linux>>:-Wpedantic>
		PRIVATE $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Windows>>:/wd4100>
		PRIVATE $<$<PLATFORM_ID:Windows>:/bigobj>
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
		$<$<PLATFORM_ID:Darwin>:__apple__> # Define __apple__ for MacOS
		$<$<PLATFORM_ID:Darwin>:__OPENGL_4_1__> # MacOS only supports up to GL4.1
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
	)
	if (APPLE)
    set_target_properties(${name}
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/Frameworks"
        INSTALL_RPATH "@executable_path/../Frameworks"
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )
	else()
		set_target_properties(${name}
	    	PROPERTIES
	    	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/lib"
		)
	endif()
endmacro()

macro(default_options name)
	default_options_local(${name})

	if (NOT APPLE)
		install(TARGETS ${name} EXPORT xstudio
        	LIBRARY DESTINATION share/xstudio/lib)
	endif()

	target_include_directories(${name} INTERFACE
  		$<BUILD_INTERFACE:${ROOT_DIR}/include>
  		$<BUILD_INTERFACE:${ROOT_DIR}/extern/include>
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
	)

	if (APPLE)
		set_target_properties(${name}
	    	PROPERTIES
	    	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/PlugIns/xstudio"
		)
		#InstallSymlink(${CMAKE_INSTALL_PREFIX}/xstudio.bin.app/Contents/Frameworks lib${name}.dylib
		#		${CMAKE_INSTALL_PREFIX}/xstudio.bin.app/Contents/Resources/share/xstudio/plugin/lib${name}.dylib
		#	)

	else()
		set_target_properties(${name}
	    	PROPERTIES
	    	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugin"
		)

		install(TARGETS ${name}
    	    LIBRARY DESTINATION share/xstudio/plugin)

	endif()


	if(WIN32)

		#This will unfortunately also install the plugin in the /bin directory.  TODO: Figure out how to omit the plugin itself.
		install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin )
		# We don't want the vcpkg install because it forces dependences; we just want the plugin.
		_install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION share/xstudio/plugin)

		#For interactive debugging, we want only the output dll to be copied to the build plugins folder.
		add_custom_command(
				TARGET ${PROJECT_NAME}
				POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${CMAKE_CURRENT_BINARY_DIR}/plugin"
		)
	endif()

endmacro()

macro(add_plugin_qml name _dir)

	add_custom_target(${name}_COPY_QML ALL)
	file(GLOB DIRS ${CMAKE_CURRENT_SOURCE_DIR}/${_dir}/* LIST_FOLDERS)

	if (APPLE)
		foreach(DIR ${DIRS})
			if(IS_DIRECTORY ${DIR})
				cmake_path(GET DIR FILENAME dirname)
				add_custom_command(TARGET ${name}_COPY_QML POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E
						copy_directory ${DIR} ${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/PlugIns/xstudio/qml/${dirname})
			endif()
		endforeach()
	else()
		foreach(DIR ${DIRS})
			if(IS_DIRECTORY ${DIR})
				cmake_path(GET DIR FILENAME dirname)
				add_custom_command(TARGET ${name}_COPY_QML POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E
						copy_directory ${DIR} ${CMAKE_BINARY_DIR}/bin/plugin/qml/${dirname})
				install(DIRECTORY ${DIR} DESTINATION share/xstudio/plugin/qml)
			endif()
		endforeach()
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
	        ${Qt6Core_INCLUDE_DIRS}
	        ${Qt6OpenGL_INCLUDE_DIRS}
	        ${Qt6Quick_INCLUDE_DIRS}
	        ${Qt6Gui_INCLUDE_DIRS}
	        ${Qt6Widgets_INCLUDE_DIRS}
	        ${Qt6OpenGLWidgets_INCLUDE_DIRS}
	        ${Qt6Concurrent_INCLUDE_DIRS}
	        ${Qt6Qml_INCLUDE_DIRS}
	)

	if (APPLE)
		set_target_properties(${name}
	    	PROPERTIES
	    	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/Frameworks"
		)
	else()
		set_target_properties(${name}
	    	PROPERTIES
	    	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/lib"
		)
		install(TARGETS ${name} EXPORT xstudio
        	LIBRARY DESTINATION share/xstudio/lib)
	endif()

endmacro()

macro(add_src_and_test_main NAME INSTALL_BIN INSTALL_PYTHON)
	if(${INSTALL_BIN})
		if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}/CMakeLists.txt)
			add_subdirectory(${NAME})
		endif()

		if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}/src)
			add_subdirectory(${NAME}/src)
		endif()

		if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}/share/preference)
			file(GLOB PREFFILES ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}/share/preference/*.json)

			STRING(REGEX REPLACE "/" "_" SAFENAME ${NAME})

			add_custom_target(${SAFENAME}_PREFERENCES ALL)
			foreach(PREFFile ${PREFFILES})
				get_filename_component(PREFNAME ${PREFFile} NAME)
				add_preference(${PREFNAME} ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}/share/preference ${SAFENAME}_PREFERENCES)
			endforeach()
		endif ()

		if (BUILD_TESTING)
			if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}/test)
				add_subdirectory(${NAME}/test)
			endif()
		endif()
	endif()

	if(${INSTALL_PYTHON})
		if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}/python)
			add_subdirectory(${NAME}/python)
		endif()
	endif()

endmacro()

macro(add_src_and_test NAME)
	add_src_and_test_main(${NAME} ${INSTALL_XSTUDIO} ${INSTALL_PYTHON_MODULE})
endmacro()

macro(add_src_and_test_always NAME)
	add_src_and_test_main(${NAME} "ON" "ON")
endmacro()


macro(add_python_plugin NAME)

	install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${NAME} DESTINATION share/xstudio/plugin-python)

	add_custom_target(COPY_PY_PLUGIN_${NAME} ALL)

 	if (APPLE)

		# ensure we have a destination directory
		add_custom_command(TARGET COPY_PY_PLUGIN_${NAME} POST_BUILD
        	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/Resources/plugin-python")

		add_custom_command(TARGET COPY_PY_PLUGIN_${NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E
        	copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/${NAME} ${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/Resources/plugin-python/${NAME})

	else()

		add_custom_command(TARGET COPY_PY_PLUGIN_${NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E
            copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/${NAME} ${CMAKE_BINARY_DIR}/bin/plugin-python/${NAME})

	endif()

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

	file(GLOB SOURCES  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.proto)

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

macro(add_resource name path target resource_type)

	if (APPLE)

		# ensure we have a destination directory
		add_custom_command(TARGET ${target} POST_BUILD
        	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/Resources/${resource_type}")

		# As part of build we copy directly into bundle resources folder
    	add_custom_command(TARGET ${target} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy ${path}/${name}
                                                    "${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/Resources/${resource_type}/")

	else()

    	add_custom_command(TARGET ${target} POST_BUILD
                   BYPRODUCTS ${CMAKE_BINARY_DIR}/bin/${resource_type}/${name}
                   COMMAND ${CMAKE_COMMAND} -E copy ${path}/${name}
                                                    ${CMAKE_BINARY_DIR}/bin/${resource_type}/${name})

	    if(INSTALL_XSTUDIO)
			install(FILES
			${path}/${name}
			DESTINATION share/xstudio/${resource_type})
    	endif ()

	endif()

endmacro()

macro(add_preference name path target)

	if (APPLE)

		# ensure we have a destination directory
		add_custom_command(TARGET ${target} POST_BUILD
        	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/Resources/preference")

		# As part of build we copy directly into bundle resources folder
    	add_custom_command(TARGET ${target} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy ${path}/${name}
                                                    "${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/Resources/preference/")

	else()

    	add_custom_command(TARGET ${target} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy ${path}/${name}
                                                    ${CMAKE_BINARY_DIR}/bin/preference/${name})

	    if(INSTALL_XSTUDIO)
			install(FILES
			${path}/${name}
			DESTINATION share/xstudio/preference)
    	endif ()

	endif()

endmacro()

macro(add_font name path target)

	if (APPLE)

		# ensure we have a destination directory
		add_custom_command(TARGET ${target} POST_BUILD
        	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/Resources/fonts")

		# As part of build we copy directly into bundle resources folder
    	add_custom_command(TARGET ${target} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy ${path}/${name}
                                                    "${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/Resources/fonts/")

	else()

    	add_custom_command(TARGET ${target} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy ${path}/${name}
                                                    ${CMAKE_BINARY_DIR}/bin/fonts/${name})

	    if(INSTALL_XSTUDIO)
			install(FILES
			${path}/${name}
			DESTINATION share/xstudio/fonts)
    	endif ()

	endif()

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

		QT6_WRAP_CPP(${MOCNAME}_moc ${MOC})

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
				endif()
			endforeach()
		endif()
	endif()
endmacro()

macro(set_python_to_proper_build_type)
	#TODO Resolve linking error when running debug build: https://github.com/pybind/pybind11/issues/3403
endmacro()


