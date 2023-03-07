FROM nvidia/cuda:11.8.0-runtime-ubuntu22.04 as devel

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/London

RUN apt-get update && apt-get install -y --no-install-recommends \
	wget build-essential git cmake

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

FROM devel as openexr
ARG JOBS=4
WORKDIR src
RUN git clone -b RB-3.1 --single-branch https://github.com/AcademySoftwareFoundation/openexr.git && \
	cd openexr && \
	mkdir build

RUN cd openexr/build && \
	cmake .. -DOPENEXR_INSTALL_TOOLS=Off -DBUILD_TESTING=Off && \
	make -j $JOBS && \
	make install 

FROM devel as actor-framework
ARG JOBS=4
WORKDIR src
RUN wget -q https://github.com/actor-framework/actor-framework/archive/refs/tags/0.18.4.tar.gz && \
	tar -xf 0.18.4.tar.gz && \
	cd actor-framework-0.18.4 && \
	./configure

RUN cd actor-framework-0.18.4/build make -j $JOBS && \
	make install

FROM devel as  ocio
WORKDIR src
ARG JOBS=4
RUN wget -q https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/refs/tags/v2.2.0.tar.gz && \
	tar -xf v2.2.0.tar.gz && \
	cd OpenColorIO-2.2.0/ && \
	mkdir build

RUN cd OpenColorIO-2.2.0/build && \
	cmake -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_TESTS=OFF -DOCIO_BUILD_GPU_TESTS=OFF ../ && \
	make -j $JOBS && \
	make install

FROM devel as  nlohmann
WORKDIR src
ARG JOBS=4
RUN wget -q https://github.com/nlohmann/json/archive/refs/tags/v3.7.3.tar.gz
RUN tar -xf v3.7.3.tar.gz && \
	cd json-3.7.3 && \
	mkdir build
RUN cd json-3.7.3/build && \
	cmake .. -DJSON_BuildTests=Off && \
	make -j $JOBS && \
	make install

FROM devel as ffmpeg
WORKDIR src
ARG JOBS=4
RUN wget -q https://ffmpeg.org/releases/ffmpeg-5.1.tar.bz2 && \
	tar -xf ffmpeg-5.1.tar.bz2 && \
	cd ffmpeg-5.1/ && \
	export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH && \
	./configure --extra-libs=-lpthread   --extra-libs=-lm    --enable-gpl   --enable-libfdk_aac   --enable-libfreetype   --enable-libmp3lame   --enable-libopus   --enable-libvpx   --enable-libx264   --enable-libx265 --enable-shared --enable-nonfree && \
	make -j  $JOBS && \
	make install

#FROM bitnami/python:3.8
#FROM ubuntu:22.04
#FROM nvidia/cuda:11.8.0-runtime-ubuntu22.04
FROM devel as main

LABEL maintainer="ling@techarge.co.uk"



#RUN apt-get update && apt-get install -y --no-install-recommends \
    #wget \
    #build-essential

#RUN apt-get install -y python3-pip \ 
	#doxygen sphinx-common sphinx-rtd-theme-common python3-breathe \
	#python-is-python3 pybind11-dev libpython3-dev \
	#libspdlog-dev libfmt-dev libssl-dev zlib1g-dev libasound2-dev nlohmann-json3-dev uuid-dev \
	#libglu1-mesa-dev freeglut3-dev mesa-common-dev libglew-dev libfreetype-dev \
	#libjpeg-dev libpulse-dev \
	#yasm nasm libfdk-aac-dev libfdk-aac2 libmp3lame-dev libopus-dev libvpx-dev libx265-dev libx264-dev \
	#qttools5-dev qtbase5-dev qt5-qmake  qtdeclarative5-dev qtquickcontrols2-5-dev \
	#qml-module-qtquick* qml-module-qt-labs-* && \
	#pip install sphinx_rtd_theme opentimelineio

# install glvnd to display OpenGL on host
RUN apt-get install -qqy libglvnd0 libgl1 libglx0 libegl1 libglvnd-dev libgl1-mesa-dev libegl1-mesa-dev
# if using jetson(aka a100?) then install this too
#RUN apt-get install -qqy libgles2 libgles2-mesa-dev

# Xlibs
RUN apt-get install -qqy libxext6 libx11-6
# optional OpenGL libs
RUN apt-get install -qqy freeglut3 freeglut3-dev


ENV NVIDIA_VISIBLE_DEVICES all
ENV NVIDIA_DRIVER_CAPABILITIES graphics,utility,compute




# OpenEXR 
WORKDIR src
#ENV JOBS=`nproc`
ENV JOBS=4
#RUN git clone -b RB-3.1 --single-branch https://github.com/AcademySoftwareFoundation/openexr.git && \
#	cd openexr/ && \
#	mkdir build && \
#	cd build && \
#	cmake .. -DOPENEXR_INSTALL_TOOLS=Off -DBUILD_TESTING=Off && \
#	make -j $JOBS && \
#	make install



# TODO: copy build artifacts to main container
COPY --from=nlohmann /usr/local/lib /usr/local/lib/
COPY --from=nlohmann /usr/local/bin /usr/local/bin/
COPY --from=ocio /usr/local/lib /usr/local/lib/
COPY --from=ocio /usr/local/bin /usr/local/bin/
COPY --from=actor-framework /usr/local/lib /usr/local/lib/
COPY --from=actor-framework /usr/local/bin /usr/local/bin/
COPY --from=openexr /usr/local/lib /usr/local/lib/
COPY --from=openexr /usr/local/bin /usr/local/bin/
COPY --from=ffmpeg /usr/local/lib /usr/local/lib/
COPY --from=ffmpeg /usr/local/bin /usr/local/bin/

# xStudio
ENV PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:/usr/local/lib64/pkgconfig
ARG JOBS=4
WORKDIR xstudio
COPY . .
RUN mkdir build && \
	cd build && \
	cmake .. -DBUILD_DOCS=Off  && \
    make -j $JOBS

ENV QV4_FORCE_INTERPRETER=1 
ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64
ENV PYTHONPATH=./bin/python/lib/python3.10/site-packages:/home/xstudio/.local/lib/python3.10/site-packages

RUN apt-get install -qqy x11-apps

# TODO: correct port number to expose
EXPOSE 8000

ENV DISPLAY=:0

CMD ./build/bin/xstudio.bin

