# syntax=docker/dockerfile:1.7
#
# xSTUDIO dependency image — Ubuntu 24.04
# Output: xstudio-deps:ubuntu2404
#
# One Docker layer per source-built dependency installed under /usr/local.
# dnf only supplies the toolchain and libs we do not vendor
# (X11/GL, Boost, lame/opus/vpx…).
#
# Version pins: versions.env.
# (ARG below have no defaults — do not put version numbers here).
# Each ARG sits immediately before its RUN so bumping one pin only
# invalidates that layer and the ones after it.

ARG UBUNTU_IMAGE=docker.io/ubuntu:24.04

FROM ${UBUNTU_IMAGE} AS deps

ENV DEBIAN_FRONTEND=noninteractive \
    DISTRO=ubuntu2404 \
    QT_ROOT=/opt/qt

# Build parallelism (empty -> 1 in lib.sh).
ARG JOBS=
ENV JOBS=${JOBS}

# ---------------------------------------------------------------------------
# 1) System packages: toolchain + libs we do not build from source
# ---------------------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential git wget bzip2 xz-utils ca-certificates pkg-config \
        autoconf automake libtool \
        python3 python3-pip python3-dev python-is-python3 libpython3-dev \
        pybind11-dev \
        libssl-dev zlib1g-dev libasound2-dev uuid-dev \
        libglu1-mesa-dev libglut-dev mesa-common-dev libfreetype-dev \
        libgl1-mesa-dev libegl1-mesa-dev \
        libpulse-dev \
        libboost-dev libboost-filesystem-dev libboost-thread-dev \
        libtiff-dev libpng-dev libgif-dev libexpat-dev \
        libmp3lame-dev libopus-dev libvpx-dev \
    && rm -rf /var/lib/apt/lists/*

# Shared helpers (cmake_build, fetch, …)
COPY scripts/lib.sh /build/scripts/lib.sh

# ---------------------------------------------------------------------------
# 2) Build tools: CMake, Ninja, pybind11, Qt 6 (aqtinstall)
# ---------------------------------------------------------------------------
ARG CMAKE_VERSION
ARG NINJA_VERSION
ARG PYBIND11_VERSION
ARG AQTINSTALL_VERSION
ARG QT_VERSION
ENV CMAKE_VERSION=${CMAKE_VERSION} \
    NINJA_VERSION=${NINJA_VERSION} \
    PYBIND11_VERSION=${PYBIND11_VERSION} \
    AQTINSTALL_VERSION=${AQTINSTALL_VERSION} \
    QT_VERSION=${QT_VERSION}
COPY scripts/build-tools.sh /build/scripts/build-tools.sh
RUN bash /build/scripts/build-tools.sh

# ---------------------------------------------------------------------------
# 3) Source dependencies — one layer each, in build order
# ---------------------------------------------------------------------------

# GLEW.
ARG GLEW_VERSION
ENV GLEW_VERSION=${GLEW_VERSION}
COPY scripts/deps/glew.sh /build/scripts/deps/glew.sh
RUN bash /build/scripts/deps/glew.sh

# nlohmann-json.
ARG JSON_VERSION
ENV JSON_VERSION=${JSON_VERSION}
COPY scripts/deps/json.sh /build/scripts/deps/json.sh
RUN bash /build/scripts/deps/json.sh

# fmt.
ARG FMT_VERSION
ENV FMT_VERSION=${FMT_VERSION}
COPY scripts/deps/fmt.sh /build/scripts/deps/fmt.sh
RUN bash /build/scripts/deps/fmt.sh

# spdlog.
ARG SPDLOG_VERSION
ENV SPDLOG_VERSION=${SPDLOG_VERSION}
COPY scripts/deps/spdlog.sh /build/scripts/deps/spdlog.sh
RUN bash /build/scripts/deps/spdlog.sh

# OpenEXR + Imath.
ARG OPENEXR_REF
ENV OPENEXR_REF=${OPENEXR_REF}
COPY scripts/deps/openexr.sh /build/scripts/deps/openexr.sh
RUN bash /build/scripts/deps/openexr.sh

# C++ Actor Framework.
ARG CAF_VERSION
ENV CAF_VERSION=${CAF_VERSION}
COPY scripts/deps/caf.sh /build/scripts/deps/caf.sh
RUN bash /build/scripts/deps/caf.sh

# OpenColorIO.
ARG OCIO_VERSION
ENV OCIO_VERSION=${OCIO_VERSION}
COPY scripts/deps/ocio.sh /build/scripts/deps/ocio.sh
RUN bash /build/scripts/deps/ocio.sh

# OpenTimelineIO.
ARG OTIO_REF
ENV OTIO_REF=${OTIO_REF}
COPY scripts/deps/otio.sh /build/scripts/deps/otio.sh
RUN bash /build/scripts/deps/otio.sh

# FFmpeg codecs: nasm, x264, x265, fdk-aac.
ARG NASM_VERSION
ARG X264_REF
ARG X265_VERSION
ARG FDKAAC_REF
ENV NASM_VERSION=${NASM_VERSION} \
    X264_REF=${X264_REF} \
    X265_VERSION=${X265_VERSION} \
    FDKAAC_REF=${FDKAAC_REF}
COPY scripts/deps/codecs.sh /build/scripts/deps/codecs.sh
RUN bash /build/scripts/deps/codecs.sh

# FFmpeg.
ARG FFMPEG_VERSION
ENV FFMPEG_VERSION=${FFMPEG_VERSION}
COPY scripts/deps/ffmpeg.sh /build/scripts/deps/ffmpeg.sh
RUN bash /build/scripts/deps/ffmpeg.sh

# libjpeg-turbo (>= 2.1 required by OIIO).
ARG LIBJPEGTURBO_VERSION
ENV LIBJPEGTURBO_VERSION=${LIBJPEGTURBO_VERSION}
COPY scripts/deps/libjpegturbo.sh /build/scripts/deps/libjpegturbo.sh
RUN bash /build/scripts/deps/libjpegturbo.sh

# LibRaw.
ARG LIBRAW_VERSION
ENV LIBRAW_VERSION=${LIBRAW_VERSION}
COPY scripts/deps/libraw.sh /build/scripts/deps/libraw.sh
RUN bash /build/scripts/deps/libraw.sh

# libde265.
ARG LIBDE265_VERSION
ENV LIBDE265_VERSION=${LIBDE265_VERSION}
COPY scripts/deps/libde265.sh /build/scripts/deps/libde265.sh
RUN bash /build/scripts/deps/libde265.sh

# libheif.
ARG LIBHEIF_VERSION
ENV LIBHEIF_VERSION=${LIBHEIF_VERSION}
COPY scripts/deps/libheif.sh /build/scripts/deps/libheif.sh
RUN bash /build/scripts/deps/libheif.sh

# OpenImageIO.
ARG OIIO_VERSION
ENV OIIO_VERSION=${OIIO_VERSION}
COPY scripts/deps/oiio.sh /build/scripts/deps/oiio.sh
RUN bash /build/scripts/deps/oiio.sh

# fileseq C++.
ARG FILESEQ_CPP_REF
ENV FILESEQ_CPP_REF=${FILESEQ_CPP_REF}
COPY scripts/deps/fileseq.sh /build/scripts/deps/fileseq.sh
RUN bash /build/scripts/deps/fileseq.sh

# Exiv2.
ARG EXIV2_VERSION
ENV EXIV2_VERSION=${EXIV2_VERSION}
COPY scripts/deps/exiv2.sh /build/scripts/deps/exiv2.sh
RUN bash /build/scripts/deps/exiv2.sh
