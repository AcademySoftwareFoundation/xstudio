find_package(Imath)

SET(LINK_DEPS
	xstudio::colour_pipeline
	xstudio::module
	Imath::Imath
)

create_plugin_with_alias(viewer_solarise_effect xstudio::colour_pipeline::viewer_solarise_effect 0.1.0  "${LINK_DEPS}")
