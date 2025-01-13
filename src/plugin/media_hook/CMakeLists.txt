# only build default hook if STUDIO_PLUGINS is
# an empty string
if("${STUDIO_PLUGINS}" STREQUAL "")
    add_src_and_test(default_hook)
endif()

build_studio_plugins("${STUDIO_PLUGINS}")