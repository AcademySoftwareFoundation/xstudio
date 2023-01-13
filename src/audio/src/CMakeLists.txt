project(audio_output VERSION 0.1.0 LANGUAGES CXX)
find_package(ALSA REQUIRED)
find_package(PulseAudio REQUIRED)

set(SOURCES
	audio_output.cpp
	audio_output_actor.cpp
)

if (WIN32)
  # TODO
elseif(APPLE)
  # TODO
else()
	list(APPEND SOURCES linux_audio_output_device.cpp)
endif()

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(xstudio::audio_output ALIAS ${PROJECT_NAME})

default_options(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::utility
		xstudio::media_reader
		caf::core
        pulse
		pulse-simple
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)