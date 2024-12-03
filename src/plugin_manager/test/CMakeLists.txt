include(CTest)

add_executable(plugin_manager_test plugin_manager_test.cpp)
default_options_gtest(plugin_manager_test)
target_link_libraries(plugin_manager_test
	PRIVATE
		xstudio::plugin_manager
		xstudio::json_store
		xstudio::global_store
		xstudio::media
		${GTEST_LDFLAGS}
)
target_compile_definitions(plugin_manager_test
	PRIVATE PLUGIN_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\"
)

add_test(plugin_manager_tests plugin_manager_test)

add_dependencies(plugin_manager_test plugin_test)


add_executable(plugin_manager_actor_test plugin_manager_actor_test.cpp)
default_options_gtest(plugin_manager_actor_test)
target_link_libraries(plugin_manager_actor_test
	PRIVATE
		xstudio::plugin_manager
		xstudio::global
		xstudio::json_store
		xstudio::global_store
		${GTEST_LDFLAGS}
)
add_test(plugin_manager_tests plugin_manager_actor_test)

set_target_properties(plugin_manager_actor_test PROPERTIES LINK_DEPENDS_NO_SHARED true)
set_target_properties(plugin_manager_test PROPERTIES LINK_DEPENDS_NO_SHARED true)

add_library(plugin_test SHARED plugin_test.cpp)
default_options(plugin_test)
target_link_libraries(plugin_test
	PUBLIC
		xstudio::plugin_manager
		xstudio::utility
		caf::core
)
set_target_properties(${name}
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugin"
)
set_target_properties(plugin_test
	PROPERTIES
	LINK_DEPENDS_NO_SHARED true
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

