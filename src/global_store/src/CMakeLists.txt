SET(LINK_DEPS
	xstudio::json_store
	CAF::core
)

SET(STATIC_LINK_DEPS
	xstudio::json_store_static
	CAF::core
)

if(UNIX AND NOT APPLE)
    list(APPEND LINK_DEPS stdc++fs)
    list(APPEND STATIC_LINK_DEPS stdc++fs)
endif()

create_component_static(global_store ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}" "${STATIC_LINK_DEPS}")