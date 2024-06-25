project(audio_output VERSION 0.1.0 LANGUAGES CXX)
if(WIN32)
    # Additional Windows-specific configuration here.
elseif(APPLE)
    # TODO: Apple-specific configuration here.
else()
    find_package(ALSA REQUIRED)
    find_package(PulseAudio REQUIRED)
endif()

set(SOURCES
    audio_output.cpp
    audio_output_actor.cpp
)

if(WIN32)
    list(APPEND SOURCES windows_audio_output_device.cpp)
elseif(APPLE)
    # TODO: Apple-specific configuration here.
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
)

if(WIN32)
    # Link against Windows Core Audio libraries.
    target_link_libraries(${PROJECT_NAME} PUBLIC "avrt.lib" "mmdevapi.lib")
elseif(APPLE)
    # TODO: Apple-specific audio libs
else()
    target_link_libraries(${PROJECT_NAME} PUBLIC pulse pulse-simple)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)
