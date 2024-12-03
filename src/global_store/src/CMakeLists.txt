SET(LINK_DEPS
	xstudio::json_store
	caf::core
)

SET(STATIC_LINK_DEPS
	xstudio::json_store_static
	caf::core
)

if(UNIX AND NOT APPLE)
    list(APPEND LINK_DEPS stdc++fs)
    list(APPEND STATIC_LINK_DEPS stdc++fs)
endif()

create_component_static(global_store 0.1.0 "${LINK_DEPS}" "${STATIC_LINK_DEPS}")