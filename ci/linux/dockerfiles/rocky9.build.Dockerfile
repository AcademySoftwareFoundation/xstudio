# syntax=docker/dockerfile:1.7
#
# xSTUDIO compile check — Rocky Linux 9
# Starts FROM the deps image (xstudio-deps:rocky9).

ARG DEPS_IMAGE=xstudio-deps:rocky9

FROM ${DEPS_IMAGE} AS compile

# Compile parallelism (forwarded to cmake --build).
ARG JOBS=
ENV JOBS=${JOBS}

# Blackmagic DeckLink SDI plugin (OFF on cloud runners / no card).
ARG BMD_DECKLINK=OFF
ENV BMD_DECKLINK=${BMD_DECKLINK}

# Studio plugin tree to build (src/plugin/*/<studio>/). Empty = none.
ARG STUDIO_PLUGINS=
ENV STUDIO_PLUGINS=${STUDIO_PLUGINS}

# Qt link-time libs kept out of the deps image (stable deps cache).
RUN dnf -y install libxkbcommon-devel \
    && dnf clean all

# Build scripts (10-build-xstudio.sh, lib.sh, …).
COPY ci/linux/scripts/ /build/scripts/

# xSTUDIO sources from the current checkout.
COPY . /src/xstudio

# cmake configure + build.
RUN bash /build/scripts/10-build-xstudio.sh
