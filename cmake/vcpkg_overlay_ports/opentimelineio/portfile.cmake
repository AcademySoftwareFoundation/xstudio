vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO AcademySoftwareFoundation/OpenTimelineIO
    REF v${VERSION}
    SHA512 305d63730446c3b4c368cadd9d7a66de96dafee2168d589ae88a0320319f40cde4a739c9939eb088b635185cb1aabd051360ed432fde3ce11ef145e18c25dd21
    HEAD_REF main
    PATCHES
        0001-disable-src-deps-subdir.patch
)

vcpkg_from_github(
    OUT_SOURCE_PATH RAPIDJSON_SOURCE_PATH
    REPO Tencent/rapidjson
    REF 06d58b9e848c650114556a23294d0b6440078c61
    SHA512 f0a7df46234e5b3244a801ddf1daefd26aac7ae5b2c470b8c3898f65c65591f6c9cabac0421800588826da9d3bcccba1f98e1c0c8c15184b3843cf6f3ffbdcad
    HEAD_REF master
)

file(COPY "${RAPIDJSON_SOURCE_PATH}/include"
     DESTINATION "${SOURCE_PATH}/src/deps/rapidjson")

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" OTIO_SHARED)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DOTIO_SHARED_LIBS=${OTIO_SHARED}
        -DOTIO_PYTHON_INSTALL=OFF
        -DOTIO_DEPENDENCIES_INSTALL=OFF
        -DOTIO_FIND_IMATH=ON
        -DOTIO_CXX_INSTALL=ON
        -DOTIO_AUTOMATIC_SUBMODULES=OFF
        -DOTIO_INSTALL_COMMANDLINE_TOOLS=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME opentimelineio
    CONFIG_PATH share/opentimelineio
)
vcpkg_cmake_config_fixup(
    PACKAGE_NAME opentime
    CONFIG_PATH share/opentime
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

# OTIO's upstream CMake installs shared libraries (.dll) into lib/ on Windows,
# but vcpkg's convention (and applocal.ps1's search path) expects DLLs in bin/.
# Move them so they get picked up by dependency deployment, and fix up the
# generated CMake targets files so IMPORTED_LOCATION still resolves.
if(VCPKG_TARGET_IS_WINDOWS AND VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/bin")
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/bin")
    file(GLOB _otio_release_dlls "${CURRENT_PACKAGES_DIR}/lib/*.dll")
    foreach(_dll ${_otio_release_dlls})
        get_filename_component(_name "${_dll}" NAME)
        file(RENAME "${_dll}" "${CURRENT_PACKAGES_DIR}/bin/${_name}")
    endforeach()
    file(GLOB _otio_debug_dlls "${CURRENT_PACKAGES_DIR}/debug/lib/*.dll")
    foreach(_dll ${_otio_debug_dlls})
        get_filename_component(_name "${_dll}" NAME)
        file(RENAME "${_dll}" "${CURRENT_PACKAGES_DIR}/debug/bin/${_name}")
    endforeach()

    # Rewrite the generated Targets-*.cmake files to reference the new DLL
    # locations. Only the .dll paths are changed; .lib (import library) paths
    # stay in lib/ where they belong.
    file(GLOB _otio_targets_files
        "${CURRENT_PACKAGES_DIR}/share/opentime/OpenTimeTargets-*.cmake"
        "${CURRENT_PACKAGES_DIR}/share/opentimelineio/OpenTimelineIOTargets-*.cmake"
    )
    foreach(_file ${_otio_targets_files})
        file(READ "${_file}" _content)
        string(REGEX REPLACE "/lib/([^/\"]*\\.dll)" "/bin/\\1" _content "${_content}")
        file(WRITE "${_file}" "${_content}")
    endforeach()
endif()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
