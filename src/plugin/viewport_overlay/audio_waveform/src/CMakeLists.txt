
SET(LINK_DEPS
	xstudio::module
	xstudio::plugin_manager
	xstudio::ui::opengl::viewport
	Imath::Imath
)

find_package(Imath)

create_plugin_with_alias(audio_waveform_overlay xstudio::viewport::audio_waveform_overlay ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")
