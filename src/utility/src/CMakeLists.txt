find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(Imath REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(UUID REQUIRED uuid)

SET(LINK_DEPS
	PUBLIC
	caf::core
	fmt::fmt
	Imath::Imath	
	nlohmann_json::nlohmann_json
	spdlog::spdlog
	stdc++fs
	uuid
	ZLIB::ZLIB
)

SET(STATIC_LINK_DEPS
	caf::core
	fmt::fmt
	Imath::Imath
	nlohmann_json::nlohmann_json
	spdlog::spdlog
	stdc++fs
	uuid
	ZLIB::ZLIB
)

find_package(ZLIB REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(UUID REQUIRED uuid)

create_component_static(utility 0.1.0 "${LINK_DEPS}" "${STATIC_LINK_DEPS}")