FROM nvidia/cuda:11.8.0-runtime-ubuntu22.04 as devel

RUN apt-get update && apt-get install -y --no-install-recommends \
	wget build-essential git cmake

FROM devel as openexr
ARG JOBS=4
RUN git clone -b RB-3.1 --single-branch https://github.com/AcademySoftwareFoundation/openexr.git && \ 
	cd openexr/ && \
	mkdir build && \
	cd build && \
	cmake .. -DOPENEXR_INSTALL_TOOLS=Off -DBUILD_TESTING=Off && \
	make -j $JOBS && \
	make install 

#FROM bitnami/python:3.8
#FROM ubuntu:22.04
#FROM nvidia/cuda:11.8.0-runtime-ubuntu22.04
FROM devel

LABEL maintainer="ling@techarge.co.uk"

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/London


#RUN apt-get update && apt-get install -y --no-install-recommends \
    #wget \
    #build-essential

RUN apt-get install -y python3-pip \ 
	doxygen sphinx-common sphinx-rtd-theme-common python3-breathe \
	python-is-python3 pybind11-dev libpython3-dev \
	libspdlog-dev libfmt-dev libssl-dev zlib1g-dev libasound2-dev nlohmann-json3-dev uuid-dev \
	libglu1-mesa-dev freeglut3-dev mesa-common-dev libglew-dev libfreetype-dev \
	libjpeg-dev libpulse-dev \
	yasm nasm libfdk-aac-dev libfdk-aac2 libmp3lame-dev libopus-dev libvpx-dev libx265-dev libx264-dev \
	qttools5-dev qtbase5-dev qt5-qmake  qtdeclarative5-dev qtquickcontrols2-5-dev \
	qml-module-qtquick* qml-module-qt-labs-* && \
	pip install sphinx_rtd_theme opentimelineio

# OpenEXR 
WORKDIR src
#ENV JOBS=`nproc`
ENV JOBS=4
RUN git clone -b RB-3.1 --single-branch https://github.com/AcademySoftwareFoundation/openexr.git && \ 
	cd openexr/ && \
	mkdir build && \
	cd build && \
	cmake .. -DOPENEXR_INSTALL_TOOLS=Off -DBUILD_TESTING=Off && \
	make -j $JOBS && \
	make install 

RUN cd /src && wget -q https://github.com/actor-framework/actor-framework/archive/refs/tags/0.18.4.tar.gz && \
	tar -xf 0.18.4.tar.gz && \
	cd actor-framework-0.18.4 && \
	./configure && \
	cd build && \
	make -j $JOBS && \
	make install 

# OCIO2
RUN cd /src && wget -q https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/refs/tags/v2.2.0.tar.gz &&\ 
	tar -xf v2.2.0.tar.gz &&\ 
	cd OpenColorIO-2.2.0/ &&\ 
	mkdir build &&\ 
	cd build &&\ 
	cmake -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_TESTS=OFF -DOCIO_BUILD_GPU_TESTS=OFF ../ &&\ 
	make -j $JOBS &&\ 
	make install

# NLOHMANN JSON
RUN wget -q https://github.com/nlohmann/json/archive/refs/tags/v3.7.3.tar.gz
RUN tar -xf v3.7.3.tar.gz && \
	cd json-3.7.3 && \
	mkdir build && \
	cd build && \
	cmake .. -DJSON_BuildTests=Off && \
	make -j $JOBS && \
	make install 

# FFMPEG 
RUN wget -q https://ffmpeg.org/releases/ffmpeg-5.1.tar.bz2 && \
	tar -xf ffmpeg-5.1.tar.bz2 && \
	cd ffmpeg-5.1/ && \
	export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH && \
	./configure --extra-libs=-lpthread   --extra-libs=-lm    --enable-gpl   --enable-libfdk_aac   --enable-libfreetype   --enable-libmp3lame   --enable-libopus   --enable-libvpx   --enable-libx264   --enable-libx265 --enable-shared --enable-nonfree && \
	make -j  $JOBS && \
	make install 

# xStudio
ENV PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:/usr/local/lib64/pkgconfig

WORKDIR xstudio
COPY . .
RUN mkdir build && \
	cd build && \
	cmake .. -DBUILD_DOCS=Off  && \
    make -j 20

ENV QV4_FORCE_INTERPRETER=1 
ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64
ENV PYTHONPATH=./bin/python/lib/python3.10/site-packages:/home/xstudio/.local/lib/python3.10/site-packages

RUN apt-get install -qqy x11-apps
EXPOSE 8000

ENV DISPLAY :0

CMD ./build/bin/xstudio.bin




