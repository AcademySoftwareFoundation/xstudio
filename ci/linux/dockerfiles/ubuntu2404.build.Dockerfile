# syntax=docker/dockerfile:1.7
#
# xSTUDIO build check — Ubuntu 24.04
# Starts FROM the deps image (xstudio-deps:ubuntu2404).

ARG DEPS_IMAGE=xstudio-deps:ubuntu2404

FROM ${DEPS_IMAGE} AS build

# Build parallelism (forwarded to cmake --build / ctest).
ARG JOBS=
ENV JOBS=${JOBS}

# Blackmagic DeckLink SDI plugin (OFF on cloud runners / no card).
ARG BMD_DECKLINK=OFF
ENV BMD_DECKLINK=${BMD_DECKLINK}

# Studio plugin tree to build (src/plugin/*/<studio>/). Empty = none.
ARG STUDIO_PLUGINS=
ENV STUDIO_PLUGINS=${STUDIO_PLUGINS}

# Build and register GTest unit tests when ON.
ARG BUILD_TESTING=OFF
ENV BUILD_TESTING=${BUILD_TESTING}

# Qt link-time libs + gtest (kept out of the deps image).
RUN apt-get update && apt-get install -y --no-install-recommends \
        libxkbcommon-dev libfontconfig-dev \
        libgtest-dev \
    && rm -rf /var/lib/apt/lists/*

# Build scripts (build-xstudio.sh, run-tests.sh, lib.sh, …).
COPY ci/linux/scripts/ /build/scripts/

# xSTUDIO sources from the current checkout.
COPY . /src/xstudio

# cmake configure + build.
RUN bash /build/scripts/build-xstudio.sh

FROM build AS test
RUN bash /build/scripts/run-tests.sh
