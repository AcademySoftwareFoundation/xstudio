
macro(default_compile_options name)
	target_compile_options(${name}
		# PRIVATE -fvisibility=hidden
		PRIVATE $<$<AND:$<CONFIG:RelWithDebInfo>,$<PLATFORM_ID:Linux>>:-fno-omit-frame-pointer>
		PRIVATE $<$<AND:$<CONFIG:RelWithDebInfo>,$<PLATFORM_ID:Windows>>:/Oy>
		# PRIVATE $<$<CONFIG:Debug>:-Wno-unused-variable>
		# PRIVATE $<$<CONFIG:Debug>:-Wno-unused-but-set-variable>
		# PRIVATE $<$<CONFIG:Debug>:-Wno-unused-parameter>
		PRIVATE $<$<CONFIG:Debug>:
			$<$<PLATFORM_ID:Linux>:-Wno-unused-function>
			$<$<PLATFORM_ID:Windows>:/wd4100>
		>
		# PRIVATE $<$<CONFIG:Debug>:-Wall>
		# PRIVATE $<$<CONFIG:Debug>:-Werror>
		$<$<PLATFORM_ID:Linux>:PRIVATE $<$<CONFIG:Debug>:-Wno-unused-function>>
		$<$<PLATFORM_ID:Linux>:PRIVATE $<$<CONFIG:Debug>:-Wextra>>
		$<$<PLATFORM_ID:Linux>:PRIVATE $<$<CONFIG:Debug>:-Wpedantic>>
		# PRIVATE ${GTEST_CFLAGS}
	)

	target_compile_features(${name}
		PUBLIC cxx_std_17
	)

	target_compile_definitions(${name}
		PUBLIC $<$<BOOL:${BUILD_TESTING}>:test_private=public>
		PUBLIC $<$<NOT:$<BOOL:${BUILD_TESTING}>>:test_private=private>
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
	)
endmacro()

if (BUILD_TESTING)
	macro(default_compile_options_gtest name)
		target_compile_options(${name}
			# PRIVATE -fvisibility=hidden
			PRIVATE $<$<CONFIG:RelWithDebInfo>:-fno-omit-frame-pointer>
			# PRIVATE $<$<CONFIG:Debug>:-Wno-unused-variable>
			# PRIVATE $<$<CONFIG:Debug>:-Wno-unused-but-set-variable>
			# PRIVATE $<$<CONFIG:Debug>:-Wno-unused-parameter>
			$<$<PLATFORM_ID:Linux>:PRIVATE $<$<CONFIG:Debug>:-Wno-unused-function>>
			# PRIVATE $<$<CONFIG:Debug>:-Wall>
			# PRIVATE $<$<CONFIG:Debug>:-Werror>
			$<$<PLATFORM_ID:Linux>:PRIVATE $<$<CONFIG:Debug>:-Wextra>>
			$<$<PLATFORM_ID:Linux>:PRIVATE $<$<CONFIG:Debug>:-Wpedantic>>
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
  		$<INSTALL_INTERFACE:include>
		$<INSTALL_INTERFACE:extern/include>)
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

	find_package(spdlog REQUIRED)
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
	    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugin"
	)
	install(TARGETS ${name}
        LIBRARY DESTINATION share/xstudio/plugin)
endmacro()

if (BUILD_TESTING)
	macro(default_options_gtest name)
		find_package(PkgConfig)
		pkg_search_module(GTEST REQUIRED gtest_main)
		if (NOT CAF_FOUND)
			find_package(CAF COMPONENTS core io)
		endif (NOT CAF_FOUND)
		find_package(spdlog REQUIRED)
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
	find_package(spdlog REQUIRED)
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

	if(WIN32) #TODO: Determine if we need to keep this limited to win32.
	# We don't want the vcpkg install.

	#This will unfortunately also install the plugin in the /bin directory.  TODO: Figure out how to omit the plugin itself.
	install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin )


	_install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/plugin)
	endif()


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
				endif()
			endforeach()

		endif()

	endif()

endmacro()


macro(set_python_to_proper_build_type)
		if(WIN32)
			#target_compile_definitions(${PROJECT_NAME} PUBLIC "$<$<CONFIG:Debug>:-DPy_DEBUG")
			#target_compile_definitions(${PROJECT_NAME} PUBLIC "$<$<CONFIG:Debug>:PYTHON_IS_DEBUG=0>")
			#add_compile_definitions(PY_NO_LINK_LIB)
			set_property(TARGET ${PROJECT_NAME} PROPERTY EXCLUDE_FROM_DEFAULT_BUILD_DEBUG TRUE)

		endif()
endmacro()


