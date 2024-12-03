find_package(OpenSSL)
find_package(ZLIB)


SET(LINK_DEPS
	xstudio::utility
	xstudio::media
	caf::core
	OpenSSL::SSL
	ZLIB::ZLIB
)

create_component(http_client 0.1.0 "${LINK_DEPS}")

# project(http_client VERSION 0.1.0 LANGUAGES CXX)

# set(SOURCES
# 	http_client.cpp
# 	http_client_actor.cpp
# )


# add_library(${PROJECT_NAME} SHARED ${SOURCES})
# add_library(xstudio::http_client ALIAS ${PROJECT_NAME})
# default_options(${PROJECT_NAME})

# target_link_libraries(${PROJECT_NAME}
# 	PUBLIC
# 		xstudio::utility
# 		caf::core
# 		OpenSSL::SSL
# 		ZLIB::ZLIB
# )
