include(CTest)

add_executable(ffmpeg_test ffmpeg_test.cpp)
default_options_gtest(ffmpeg_test)
target_link_libraries(ffmpeg_test
	PUBLIC
		xstudio::media_reader::ffmpeg
		xstudio::utility
		xstudio::json_store
		xstudio::global_store
		xstudio::media
		xstudio::playhead
		xstudio::timeline
		xstudio::colour_pipeline
		xstudio::global
		fmt::fmt
		${GTEST_LDFLAGS}
)
target_include_directories(ffmpeg_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(ffmpeg_tests ffmpeg_test)

set_target_properties(ffmpeg_test PROPERTIES LINK_DEPENDS_NO_SHARED true)

add_executable(ffmpeg_audio_integrity_test ffmpeg_audio_integrity_test.cpp)
default_options_gtest(ffmpeg_audio_integrity_test)
target_link_libraries(ffmpeg_audio_integrity_test
	PUBLIC
		xstudio::media_reader::ffmpeg
		xstudio::utility
		xstudio::json_store
		xstudio::global_store
		xstudio::media
		xstudio::playhead
		xstudio::timeline
		xstudio::colour_pipeline
		xstudio::global
		fmt::fmt
		${GTEST_LDFLAGS}
)
target_include_directories(ffmpeg_audio_integrity_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Generate the tone media with the ffmpeg that xSTUDIO itself vendors, the same
# one src/launch/xstudio/src/CMakeLists.txt ships with the application, so the
# test needs neither checked-in media nor an ffmpeg on the host.
find_program(FFMPEG_APP ffmpeg HINTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/ffmpeg")
if(NOT FFMPEG_APP)
	message(FATAL_ERROR "ffmpeg not found in ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/ffmpeg - ffmpeg_audio_integrity_test cannot generate its test media")
endif()
target_compile_definitions(ffmpeg_audio_integrity_test
	PRIVATE XSTUDIO_TEST_FFMPEG_APP="${FFMPEG_APP}")

add_test(ffmpeg_audio_integrity_tests ffmpeg_audio_integrity_test)

set_target_properties(ffmpeg_audio_integrity_test PROPERTIES LINK_DEPENDS_NO_SHARED true)
