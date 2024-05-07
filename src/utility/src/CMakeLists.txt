find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(Imath REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(ZLIB REQUIRED)
if(WIN32)
	# Not Required
elseif(UNIX AND NOT APPLE)
	find_package(PkgConfig REQUIRED)
	pkg_search_module(UUID REQUIRED uuid)
endif()


SET(LINK_DEPS
	PUBLIC
	caf::core
	fmt::fmt
	Imath::Imath	
	nlohmann_json::nlohmann_json
	spdlog::spdlog
	uuid
	ZLIB::ZLIB
)

SET(STATIC_LINK_DEPS
	caf::core
	fmt::fmt
	Imath::Imath
	nlohmann_json::nlohmann_json
	spdlog::spdlog
	uuid
	ZLIB::ZLIB
)

if(UNIX AND NOT APPLE)
    list(APPEND LINK_DEPS stdc++fs)
	list(APPEND STATIC_LINK_DEPS stdc++fs)
endif()


create_component_static(utility 0.1.0 "${LINK_DEPS}" "${STATIC_LINK_DEPS}")