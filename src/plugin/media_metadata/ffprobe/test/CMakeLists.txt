include(CTest)

add_executable(ffprobe_test ffprobe_test.cpp)
default_options_gtest(ffprobe_test)
target_link_libraries(ffprobe_test
	PUBLIC
		xstudio::media_metadata::ffprobe
		xstudio::utility
		${GTEST_LDFLAGS}
)
target_include_directories(ffprobe_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(ffprobe_tests ffprobe_test)

set_target_properties(ffprobe_test PROPERTIES LINK_DEPENDS_NO_SHARED true)
