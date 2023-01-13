SET(LINK_DEPS
	caf::core
	fmt::fmt
	nlohmann_json::nlohmann_json
	reproc++
	spdlog::spdlog
	stdc++fs
	uuid
)

SET(STATIC_LINK_DEPS
	caf::core
	fmt::fmt
	nlohmann_json::nlohmann_json
	reproc++
	spdlog::spdlog
	stdc++fs
	uuid
)

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(UUID REQUIRED uuid)

create_component_static(utility 0.1.0 "${LINK_DEPS}" "${STATIC_LINK_DEPS}")
