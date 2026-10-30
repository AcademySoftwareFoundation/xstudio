# Distro-agnostic Linux triplet (local / docs). CI uses distro-specific
# triplets so NuGet ABI caches stay separated:
#   x64-xstudio-linux-ubuntu2404
#   x64-xstudio-linux-rocky9
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
