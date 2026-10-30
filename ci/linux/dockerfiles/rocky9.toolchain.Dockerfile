# syntax=docker/dockerfile:1.7
#
# xSTUDIO Linux toolchain image — Rocky Linux 9
# Output: xstudio-toolchain:rocky9-<contenthash>
#
# Contains: compiler, build tools, system headers, Qt via aqt.
# Third-party libs come from vcpkg + NuGet at build time inside this image.

ARG ROCKY_IMAGE=docker.io/rockylinux/rockylinux:9

FROM ${ROCKY_IMAGE} AS toolchain

ENV DISTRO=rocky9 \
    QT_ROOT=/opt/qt

ARG JOBS=
ENV JOBS=${JOBS}

RUN dnf -y install dnf-plugins-core \
    && dnf config-manager --set-enabled crb \
    && dnf -y groupinstall "Development Tools" \
    && dnf -y install \
        git wget curl ca-certificates pkgconf-pkg-config \
        zip unzip tar xz bzip2 which file \
        autoconf automake libtool nasm yasm bison flex \
        python3 python3-devel python3-pip \
        alsa-lib-devel pulseaudio-libs-devel \
        freeglut-devel mesa-libGL-devel mesa-libGLU-devel mesa-libEGL-devel libglvnd-devel \
        libuuid-devel freetype-devel \
        libxkbcommon-devel fontconfig-devel \
        zlib-devel openssl-devel expat-devel \
        libtiff-devel libpng-devel giflib-devel \
    && dnf clean all

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
