#!/usr/bin/env bash
# Build the deps image for one distro (local / VS Code).
# Usage: ci/linux/build-deps.sh ubuntu24|rocky9
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=versions.env
source "${ROOT}/versions.env"

distro="${1:?usage: $0 ubuntu24|rocky9}"
case "$distro" in
  ubuntu24)
    df="${ROOT}/dockerfiles/ubuntu2404.deps.Dockerfile"
    tag="xstudio-deps:ubuntu2404"
    ;;
  rocky9)
    df="${ROOT}/dockerfiles/rocky9.deps.Dockerfile"
    tag="xstudio-deps:rocky9"
    ;;
  *)
    echo "ERROR: unknown distro '$distro' (want ubuntu24|rocky9)" >&2
    exit 1
    ;;
esac

export DOCKER_BUILDKIT=1
docker build \
  -f "$df" \
  --build-arg "JOBS=${JOBS:-8}" \
  --build-arg "CMAKE_VERSION=${CMAKE_VERSION}" \
  --build-arg "NINJA_VERSION=${NINJA_VERSION}" \
  --build-arg "PYBIND11_VERSION=${PYBIND11_VERSION}" \
  --build-arg "AQTINSTALL_VERSION=${AQTINSTALL_VERSION}" \
  --build-arg "QT_VERSION=${QT_VERSION}" \
  --build-arg "OPENEXR_REF=${OPENEXR_REF}" \
  --build-arg "CAF_VERSION=${CAF_VERSION}" \
  --build-arg "OCIO_VERSION=${OCIO_VERSION}" \
  --build-arg "OTIO_REF=${OTIO_REF}" \
  --build-arg "FILESEQ_CPP_REF=${FILESEQ_CPP_REF}" \
  --build-arg "FFMPEG_VERSION=${FFMPEG_VERSION}" \
  --build-arg "OIIO_VERSION=${OIIO_VERSION}" \
  --build-arg "LIBRAW_VERSION=${LIBRAW_VERSION}" \
  --build-arg "LIBDE265_VERSION=${LIBDE265_VERSION}" \
  --build-arg "LIBHEIF_VERSION=${LIBHEIF_VERSION}" \
  --build-arg "EXIV2_VERSION=${EXIV2_VERSION}" \
  --build-arg "GLEW_VERSION=${GLEW_VERSION}" \
  --build-arg "JSON_VERSION=${JSON_VERSION}" \
  --build-arg "SPDLOG_VERSION=${SPDLOG_VERSION}" \
  --build-arg "FMT_VERSION=${FMT_VERSION}" \
  --build-arg "LIBJPEGTURBO_VERSION=${LIBJPEGTURBO_VERSION}" \
  --build-arg "X264_REF=${X264_REF}" \
  --build-arg "X265_VERSION=${X265_VERSION}" \
  --build-arg "FDKAAC_REF=${FDKAAC_REF}" \
  --build-arg "NASM_VERSION=${NASM_VERSION}" \
  -t "$tag" \
  "$ROOT"

echo "==> deps image OK: $tag"
