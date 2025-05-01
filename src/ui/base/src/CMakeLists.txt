if (USE_VCPKG)
	find_package(freetype CONFIG REQUIRED)
else()
	find_package(Freetype REQUIRED)
	include_directories("${FREETYPE_INCLUDE_DIRS}")
endif()
find_package(Imath)
find_package(OpenEXR)

SET(LINK_DEPS
    xstudio::utility
	Imath::Imath
	OpenEXR::OpenEXR
	freetype
)

if(UNIX)
    list(APPEND LINK_DEPS pthread)
endif()

create_component_with_alias(ui_base xstudio::ui::base ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")