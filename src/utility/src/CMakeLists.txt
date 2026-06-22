find_package(spdlog CONFIG REQUIRED)
find_package(fmt REQUIRED)
find_package(Imath REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(ZLIB REQUIRED)
# find_package(OpenSSL)

if(WIN32)
	# Not Required
elseif(UNIX AND NOT APPLE)
	find_package(PkgConfig REQUIRED)
	pkg_search_module(UUID REQUIRED uuid)
endif()


SET(LINK_DEPS
	PUBLIC
	CAF::core
	Imath::Imath
	nlohmann_json::nlohmann_json
	spdlog::spdlog
	fmt::fmt
	ZLIB::ZLIB
    # OpenSSL::SSL
)

SET(STATIC_LINK_DEPS
	CAF::core
	Imath::Imath
	nlohmann_json::nlohmann_json
	spdlog::spdlog
	fmt::fmt
	ZLIB::ZLIB
    # OpenSSL::SSL
)

if(APPLE)
	list(APPEND LINK_DEPS "-framework CoreFoundation")
	list(APPEND STATIC_LINK_DEPS "-framework CoreFoundation")
elseif(WIN32)
    list(APPEND LINK_DEPS uuid)
	list(APPEND STATIC_LINK_DEPS uuid)
else()
	# Linux!
    list(APPEND LINK_DEPS uuid)
	list(APPEND STATIC_LINK_DEPS uuid)
    list(APPEND LINK_DEPS stdc++fs)
	list(APPEND STATIC_LINK_DEPS stdc++fs)
endif()

create_component_static(utility ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}" "${STATIC_LINK_DEPS}")