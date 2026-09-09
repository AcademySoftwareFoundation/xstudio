# Windows portable (no-install) package.
#
# `cmake --build <build> --target portable` produces:
#   <build>/portable/xSTUDIO-<version>-win64/      staged, relocatable folder
#   <build>/xSTUDIO-<version>-win64-portable.zip   archive with one top-level folder
#
# Included from the top-level CMakeLists.txt (Windows only), after every
# add_subdirectory() call so the target collection below sees all targets.
# The staging/prune/zip work lives in scripts/portable/portable.cmake.in,
# configured into the build tree as portable.cmake.

set(XSTUDIO_PORTABLE_NAME "xSTUDIO-${XSTUDIO_GLOBAL_VERSION}-win64")
set(XSTUDIO_PORTABLE_STAGE "${CMAKE_BINARY_DIR}/portable/${XSTUDIO_PORTABLE_NAME}")

configure_file(scripts/portable/portable.cmake.in
    ${CMAKE_BINARY_DIR}/portable.cmake @ONLY)

add_custom_target(portable
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${XSTUDIO_PORTABLE_STAGE}"
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}"
                             --prefix "${XSTUDIO_PORTABLE_STAGE}" --config $<CONFIG>
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/portable.cmake"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Building portable xSTUDIO package"
    USES_TERMINAL VERBATIM)

# Make `portable` depend on every buildsystem target so it builds the same
# set as `--target package`. ALL membership is not enough: targets such as
# python_module and *_COPY_QML are add_custom_target(... ALL ...) and only
# run when explicitly requested or depended upon. Targets without ALL
# membership (Qt qmllint/qmlcachegen helpers, clang-tidy, clangformat,
# `portable` itself) report EXCLUDE_FROM_ALL and are skipped; the denylist is
# a belt-and-braces guard for the always-defined lint targets.
set(XSTUDIO_PORTABLE_DENYLIST portable clang-tidy clangformat)

function(_xstudio_portable_collect_targets dir)
    get_directory_property(_targets DIRECTORY "${dir}" BUILDSYSTEM_TARGETS)
    foreach(_target IN LISTS _targets)
        get_target_property(_excluded "${_target}" EXCLUDE_FROM_ALL)
        if(_excluded)
            continue()
        endif()
        get_target_property(_type "${_target}" TYPE)
        if(_type STREQUAL "INTERFACE_LIBRARY")
            continue()
        endif()
        list(FIND XSTUDIO_PORTABLE_DENYLIST "${_target}" _denied)
        if(_denied GREATER_EQUAL 0)
            continue()
        endif()
        set_property(GLOBAL APPEND PROPERTY _XSTUDIO_PORTABLE_TARGETS "${_target}")
    endforeach()

    get_directory_property(_subdirs DIRECTORY "${dir}" SUBDIRECTORIES)
    foreach(_subdir IN LISTS _subdirs)
        _xstudio_portable_collect_targets("${_subdir}")
    endforeach()
endfunction()

_xstudio_portable_collect_targets("${CMAKE_SOURCE_DIR}")
get_property(_xstudio_portable_targets GLOBAL PROPERTY _XSTUDIO_PORTABLE_TARGETS)
add_dependencies(portable ${_xstudio_portable_targets})
