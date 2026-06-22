find_package(spdlog CONFIG REQUIRED)
find_package(fmt REQUIRED)
find_package(Imath REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

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
)

SET(STATIC_LINK_DEPS
	CAF::core
	Imath::Imath
	nlohmann_json::nlohmann_json
	spdlog::spdlog
	fmt::fmt
	ZLIB::ZLIB
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

# OpenSSL is an implementation detail of md5_hash — keep it off the public link
# interface so downstream consumers (plugins) don't need to find_package(OpenSSL).
if(INSTALL_XSTUDIO)
	target_link_libraries(utility PRIVATE OpenSSL::Crypto)
endif()
target_link_libraries(utility_static PRIVATE OpenSSL::Crypto)