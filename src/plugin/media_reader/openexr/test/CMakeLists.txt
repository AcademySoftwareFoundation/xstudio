include(CTest)

add_executable(openexr_test openexr_test.cpp)
default_options_gtest(openexr_test)
target_link_libraries(openexr_test
	PUBLIC
		xstudio::media_reader::openexr
		${GTEST_LDFLAGS}
)
target_include_directories(openexr_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(openexr_tests openexr_test)

set_target_properties(openexr_test PROPERTIES LINK_DEPENDS_NO_SHARED true)
