include(CTest)

find_package(FFMPEG REQUIRED)

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
		${FFMPEG_LIBRARIES}
)
target_include_directories(ffmpeg_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${FFMPEG_INCLUDE_DIRS}
)
target_link_directories(ffmpeg_test PRIVATE ${FFMPEG_LIBRARY_DIRS})

add_test(ffmpeg_tests ffmpeg_test)

set_target_properties(ffmpeg_test PROPERTIES LINK_DEPENDS_NO_SHARED true)
