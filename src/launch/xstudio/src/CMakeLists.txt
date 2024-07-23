project(xstudio VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

set(SOURCES
	xstudio.cpp
	../../../../ui/qml/reskin/qml_reskin.qrc
	../../../../ui/qml/xstudio/qml.qrc
)

if(WIN32)
    # Add the /bigobj option for xstudio.cpp
    set_source_files_properties(xstudio.cpp PROPERTIES COMPILE_FLAGS "/bigobj")
endif()

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(Qt5 COMPONENTS Core Quick Gui Widgets OpenGL REQUIRED)
find_package(OpenSSL)
find_package(ZLIB)
#find_package(OpenTime REQUIRED)
#find_package(OpenTimelineIO REQUIRED)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(${PROJECT_NAME} ${SOURCES})

configure_file(.clang-tidy .clang-tidy)

if(WIN32)
	configure_file(xstudio.bat.in xstudio.bat)
else()
	configure_file(xstudio.sh.in xstudio.sh @ONLY)
endif()

default_options(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		xstudio::caf_utility
		xstudio::global
		xstudio::ui::opengl::viewport
		xstudio::ui::qml::bookmark
		xstudio::ui::qml::embedded_python
		xstudio::ui::qml::event
		xstudio::ui::qml::global_store
		xstudio::ui::qml::helper
		xstudio::ui::qml::log
		xstudio::ui::qml::module
		xstudio::ui::qml::playhead
		xstudio::ui::model_data
		xstudio::ui::qml::session
		xstudio::ui::qml::studio
		xstudio::ui::qml::tag
		xstudio::ui::qml::viewport
		xstudio::ui::viewport
		xstudio::ui::qt::viewport_widget
		xstudio::utility
	PUBLIC
		caf::core
		$<$<BOOL:${NVIDIA_HACK}>:GLdispatch>
		Qt5::Gui
		Qt5::Core
		Qt5::Qml
		Qt5::Quick
		Qt5::Widgets
		OpenSSL::SSL
		ZLIB::ZLIB
		#OTIO::opentime
		#OTIO::opentimelineio
		quickfuture
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE dbghelp)
endif()

if(WIN32)
	set_target_properties(${PROJECT_NAME}
		PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
		OUTPUT_NAME "${PROJECT_NAME}"
		VS_DEBUGGER_ENVIRONMENT XSTUDIO_ROOT=${CMAKE_BINARY_DIR}/bin/$<$<CONFIG:Debug>:Debug>$<$<CONFIG:Release>:Release>
		LINK_DEPENDS_NO_SHARED true
	)
else()
	set_target_properties(${PROJECT_NAME}
		PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
		OUTPUT_NAME "${PROJECT_NAME}.bin"	
		LINK_DEPENDS_NO_SHARED true
	)
endif()

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin)

if(WIN32)
	install(PROGRAMS
	  ${CMAKE_CURRENT_BINARY_DIR}/xstudio.bat
	  DESTINATION bin
	  RENAME xstudio)
else()
	install(PROGRAMS
	  ${CMAKE_CURRENT_BINARY_DIR}/xstudio.sh
	  DESTINATION bin
	  RENAME xstudio)

	install(PROGRAMS
	  ${CMAKE_CURRENT_SOURCE_DIR}/xstudio_desktop_integration.sh
	  DESTINATION bin
	  RENAME xstudio_desktop_integration)
endif()
