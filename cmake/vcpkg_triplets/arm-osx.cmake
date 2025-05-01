set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)

# Dynamic libs are preferred, and some cases necessary,
# but VCPKG will not build python3 and its dependencies
# as dynamic libs
if(NOT PORT MATCHES ".*bzip2.*" 
    AND NOT PORT MATCHES ".*expat.*" 
    AND NOT PORT MATCHES ".*gettext.*" 
    AND NOT PORT MATCHES ".*ffi.*" 
    AND NOT PORT MATCHES ".*iconv.*" 
    AND NOT PORT MATCHES ".*zma.*" 
    AND NOT PORT MATCHES ".*openssl.*" 
    AND NOT PORT MATCHES ".*sqlite.*" 
    AND NOT PORT MATCHES ".*zlib.*")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
    message("PORT DYNAMIC " ${PORT})
else()
    message("PORT STATIC " ${PORT})
endif()
