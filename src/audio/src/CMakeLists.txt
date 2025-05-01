project(audio_output VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)
if(WIN32)
    # Additional Windows-specific configuration here.
elseif(APPLE)
    # TODO: Apple-specific configuration here.
else()
    find_package(ALSA REQUIRED)
    find_package(PulseAudio REQUIRED)
endif()

find_package(CAF COMPONENTS core io)

set(SOURCES
    audio_output.cpp
    audio_output_actor.cpp
)

if(WIN32)
    list(APPEND SOURCES windows_audio_output_device.cpp)
elseif(APPLE)
    list(APPEND SOURCES macos_audio_output_device.cpp)
else()
    list(APPEND SOURCES linux_audio_output_device.cpp)
endif()

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(xstudio::audio_output ALIAS ${PROJECT_NAME})

default_options(${PROJECT_NAME})

SET(LINK_DEPS
    PUBLIC
        xstudio::utility
        xstudio::media_reader
        xstudio::module
        CAF::core
)

if(WIN32)
    # Link against Windows Core Audio libraries.
    list(APPEND LINK_DEPS PUBLIC "avrt.lib" "mmdevapi.lib")
elseif(APPLE)
	list(APPEND LINK_DEPS "-framework CoreFoundation" "-framework AudioToolbox")
else()
    list(APPEND LINK_DEPS PUBLIC pulse pulse-simple)
endif()

target_link_libraries(${PROJECT_NAME} ${LINK_DEPS})

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)