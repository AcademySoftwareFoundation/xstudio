include(CTest)

add_executable(blank_test blank_test.cpp)
default_options_gtest(blank_test)
target_link_libraries(blank_test
	PUBLIC
		xstudio::media_reader::blank
		${GTEST_LDFLAGS}
)
target_include_directories(blank_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(blank_tests blank_test)

set_target_properties(blank_test PROPERTIES LINK_DEPENDS_NO_SHARED true)
