SET(LINK_DEPS
    xstudio::utility
	Imath::Imath
	OpenEXR::OpenEXR
	ui_base
)

if(UNIX)
    list(APPEND LINK_DEPS pthread)
endif()

if(WIN32)
	find_package(freetype CONFIG REQUIRED)
else()
	find_package(Freetype)
	include_directories("${FREETYPE_INCLUDE_DIRS}")
endif()

find_package(Imath)
find_package(OpenEXR)

create_component_with_alias(ui_canvas xstudio::ui::canvas 0.1.0 "${LINK_DEPS}")

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		freetype
)