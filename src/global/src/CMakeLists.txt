project(global VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)
find_package(pybind11 REQUIRED)  # or `add_subdirectory(pybind11)`

set(SOURCES
	global_actor.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(xstudio::global ALIAS ${PROJECT_NAME})
default_options(${PROJECT_NAME})
if(UNIX)
target_compile_options(${PROJECT_NAME}
	PRIVATE -fvisibility=hidden
)
endif()

set_python_to_proper_build_type()

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::module
		xstudio::audio_output
		xstudio::conform
		xstudio::embedded_python
		xstudio::event
		xstudio::global_store
		xstudio::media_cache
		xstudio::media_cache
		xstudio::media_hook
		xstudio::media_metadata
		xstudio::media_reader
		xstudio::plugin_manager
		xstudio::colour_pipeline
		xstudio::scanner
		xstudio::session
		xstudio::studio
		xstudio::sync
		xstudio::thumbnail
		xstudio::ui::model_data
		xstudio::ui::viewport
		xstudio::utility
		caf::core
		caf::io
	PRIVATE
		pybind11::embed
)

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE asound)  # Link against asound on Linux
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)
