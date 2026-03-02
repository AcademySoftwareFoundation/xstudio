###############################################################
#
# Find Dependencies
#
###############################################################

project(blackmagic_decklink VERSION 0.1.0 LANGUAGES CXX)

if (WIN32)
	add_subdirectory(extern/win)
endif()

find_package(nlohmann_json REQUIRED)
find_package(CAF CONFIG COMPONENTS core REQUIRED)
find_package(OpenEXR REQUIRED)
find_package(Imath REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(Qt6 COMPONENTS Core Qml REQUIRED)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Platform-conditional DeckLink SDK dispatch source
if(APPLE)
	set(DECKLINK_DISPATCH_SRC extern/mac/DeckLinkAPIDispatch.cpp)
elseif(UNIX)
	set(DECKLINK_DISPATCH_SRC extern/linux/DeckLinkAPIDispatch.cpp)
endif()

set(SOURCES
	decklink_plugin.cpp
	decklink_output.cpp
	decklink_audio_device.cpp
	pixel_swizzler.cpp
	BlackmagicSDI/resources/decklink_plugin.qrc
	${DECKLINK_DISPATCH_SRC}
	${blackmagic_decklink_MOC_SRC}
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::plugin_manager
		xstudio::bookmark
		xstudio::utility
		xstudio::module
		xstudio::media
		xstudio::media_reader
		xstudio::audio_output
		xstudio::global_store
		xstudio::json_store
		xstudio::global
		xstudio::ui::opengl::viewport
		xstudio::ui::qml::helper
		xstudio::ui::qt::viewport_widget
		xstudio::ui::viewport
		CAF::core
		Qt6::Core
	PRIVATE
		OpenEXR::OpenEXR
		Imath::Imath
		spdlog::spdlog
		fmt::fmt
)

if (WIN32)
	target_link_libraries(${PROJECT_NAME} PRIVATE decklink_idl)
endif()

file(GLOB QML_SOURCES RELATIVE ${CMAKE_CURRENT_LIST_DIR} BlackmagicSDI/*.qml)

message("QML_SOURCES ${QML_SOURCES}  ${CMAKE_CURRENT_LIST_DIR}")

qt_add_qml_module(${PROJECT_NAME}
    URI BlackmagicSDI
	VERSION 1.0
	NO_GENERATE_PLUGIN_SOURCE 
	NO_PLUGIN_OPTIONAL 
	RESOURCE_PREFIX /
	PLUGIN_TARGET ${PROJECT_NAME}	
	QML_FILES
		BlackmagicSDI/DecklinkSettingsHorizontalWidget.qml
		BlackmagicSDI/DecklinkMultichoiceSetting.qml
		BlackmagicSDI/DecklinkStatusIndicator.qml
		BlackmagicSDI/DecklinkSettingsDialog.qml
		BlackmagicSDI/DecklinkOutputButton.qml
		BlackmagicSDI/DecklinkShelfButton.qml
		BlackmagicSDI/DecklinkMainSettings.qml
		BlackmagicSDI/DecklinkHDRSettings.qml
	)

# Platform-specific link dependencies
if(APPLE)
	target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreFoundation")
elseif(UNIX)
	target_link_libraries(${PROJECT_NAME} PRIVATE dl)
endif()

# Include directories for DeckLink SDK platform headers
target_include_directories(${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)

message("Building plugin for xSTUDIO version ${XSTUDIO_GLOBAL_VERSION}")

target_compile_definitions(${PROJECT_NAME}
		PRIVATE XSTUDIO_GLOBAL_VERSION=\"${XSTUDIO_GLOBAL_VERSION}\"
		PRIVATE PROJECT_VERSION=\"${PROJECT_VERSION}\")

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

# On macOS, xstudio headers define static globals (e.g. BroadcastActor::NAME) that also
# exist in xstudio's dylibs. Without hidden visibility, the dynamic linker resolves our
# plugin's references to the host library's __DATA_CONST copies, causing SIGBUS when our
# static initializers try to write to that read-only memory.
# Fix: hide all symbols by default so our globals stay local to the plugin.
if(APPLE)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		CXX_VISIBILITY_PRESET hidden
		VISIBILITY_INLINES_HIDDEN ON
	)
endif()

default_plugin_options(${PROJECT_NAME})

add_preference(plugin_decklink_prefs.json ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_NAME})
