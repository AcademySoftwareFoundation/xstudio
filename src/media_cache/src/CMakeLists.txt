project(media_cache VERSION 0.1.0 LANGUAGES CXX)

set(SOURCES
	media_cache_actor.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(xstudio::media_cache ALIAS ${PROJECT_NAME})

default_options(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::media
		xstudio::global_store
		caf::core
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)
