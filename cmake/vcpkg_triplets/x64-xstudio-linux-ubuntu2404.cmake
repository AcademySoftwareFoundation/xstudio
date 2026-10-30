# Ubuntu 24.04 ABI — keep this triplet name distinct from Rocky so NuGet
# binary caches are never shared across incompatible glibc versions.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
