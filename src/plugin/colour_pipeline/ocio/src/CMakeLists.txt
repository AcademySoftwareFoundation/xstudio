find_package(OpenColorIO)
find_package(OpenEXR)
find_package(Imath)
find_package(GLEW REQUIRED)

# Temporary hack - OCIO package is not found/defined by 'modern' .cmake
# package and as such cmake doesn't distinguish the OCIO headers as system
# rather than part of the project. This means the headers are analysed by
# clang tidy and it throws some errors. The following line overcomes this.
include_directories(SYSTEM ${OCIO_INCLUDE_DIRS})

SET(LINK_DEPS
	xstudio::colour_pipeline
	xstudio::module
	GLEW::GLEW
	OpenColorIO::OpenColorIO
	OpenEXR::OpenEXR
	Imath::Imath
)

create_plugin_with_alias(colour_pipeline_ocio xstudio::colour_pipeline::ocio 0.1.0  "${LINK_DEPS}")
