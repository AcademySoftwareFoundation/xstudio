include(CTest)

find_package(OpenImageIO REQUIRED)

add_executable(openimageio_test openimageio_test.cpp)
default_options_gtest(openimageio_test)
target_link_libraries(openimageio_test
	PUBLIC
		xstudio::media_reader::oiio
		xstudio::media_reader
		xstudio::utility
		xstudio::json_store
		xstudio::global_store
		xstudio::media
		xstudio::global
		OpenImageIO::OpenImageIO
		fmt::fmt
		${GTEST_LDFLAGS}
)
target_include_directories(openimageio_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(openimageio_tests openimageio_test)

set_target_properties(openimageio_test PROPERTIES LINK_DEPENDS_NO_SHARED true)
