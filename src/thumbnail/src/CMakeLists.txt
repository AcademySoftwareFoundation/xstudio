if (USE_VCPKG)

	find_package(JPEG REQUIRED)
	find_package(libjpeg-turbo CONFIG REQUIRED)

	SET(LINK_DEPS
		CAF::core
		JPEG::JPEG
		xstudio::global_store
		xstudio::media
	)

	create_component(thumbnail ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")
	target_include_directories(thumbnail PRIVATE JPEG::JPEG)
	target_link_libraries(thumbnail PRIVATE $<IF:$<TARGET_EXISTS:libjpeg-turbo::turbojpeg>,libjpeg-turbo::turbojpeg,libjpeg-turbo::turbojpeg-static>)

else()

	find_package(JPEG REQUIRED)

	SET(LINK_DEPS
		CAF::core
		JPEG::JPEG
		xstudio::global_store
		xstudio::media
	)

	create_component(thumbnail ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")

endif()


