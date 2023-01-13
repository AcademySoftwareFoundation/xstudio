SET(LINK_DEPS
    pthread
    xstudio::utility
	Imath::Imath
	OpenEXR::OpenEXR
)

find_package(Freetype)
find_package(Imath)
find_package(OpenEXR)

create_component_with_alias(ui_base xstudio::ui::base 0.1.0 "${LINK_DEPS}")

include_directories("${FREETYPE_INCLUDE_DIRS}")

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		freetype
)