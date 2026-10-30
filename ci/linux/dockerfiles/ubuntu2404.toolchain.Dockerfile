# syntax=docker/dockerfile:1.7
#
# xSTUDIO Linux toolchain image — Ubuntu 24.04
# Output: xstudio-toolchain:ubuntu2404-<contenthash>
#
# Contains: compiler, build tools, system headers, Qt via aqt.
# Third-party libs (ffmpeg, OIIO, …) are NOT baked in — they come from
# vcpkg + NuGet at build time inside this image.

ARG UBUNTU_IMAGE=docker.io/ubuntu:24.04

FROM ${UBUNTU_IMAGE} AS toolchain

ENV DEBIAN_FRONTEND=noninteractive \
    DISTRO=ubuntu2404 \
    QT_ROOT=/opt/qt

ARG JOBS=
ENV JOBS=${JOBS}

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential git wget curl ca-certificates pkg-config \
        zip unzip tar xz-utils bzip2 \
        autoconf automake autoconf-archive libtool nasm yasm bison flex \
        python3 python3-pip python3-dev python-is-python3 libpython3-dev \
        mono-complete \
        libssl-dev zlib1g-dev libasound2-dev uuid-dev \
        libglu1-mesa-dev libglut-dev mesa-common-dev libfreetype-dev \
        libgl1-mesa-dev libegl1-mesa-dev \
        libpulse-dev \
        libxkbcommon-dev libfontconfig-dev \
        libxcb-cursor0 libxcb-xinerama0 libxcb-icccm4 libxcb-image0 \
        libxcb-keysyms1 libxcb-render-util0 libxcb-shape0 libxkbcommon-x11-0 \
        libtiff-dev libpng-dev libgif-dev libexpat-dev \
    && rm -rf /var/lib/apt/lists/*

COPY scripts/build-common.sh /build/scripts/build-common.sh

ARG CMAKE_VERSION
ARG NINJA_VERSION
ARG AQTINSTALL_VERSION
ARG QT_VERSION
ENV CMAKE_VERSION=${CMAKE_VERSION} \
    NINJA_VERSION=${NINJA_VERSION} \
    AQTINSTALL_VERSION=${AQTINSTALL_VERSION} \
    QT_VERSION=${QT_VERSION}
COPY scripts/build-tools.sh /build/scripts/build-tools.sh
RUN bash /build/scripts/build-tools.sh

WORKDIR /src/xstudio
