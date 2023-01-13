include(CTest)

add_executable(ppm_test ppm_test.cpp)
default_options_gtest(ppm_test)
target_link_libraries(ppm_test
	PUBLIC
		xstudio::media_reader::ppm
		${GTEST_LDFLAGS}
)
target_include_directories(ppm_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(ppm_tests ppm_test)

set_target_properties(ppm_test PROPERTIES LINK_DEPENDS_NO_SHARED true)
