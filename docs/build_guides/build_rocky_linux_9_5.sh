#!/bin/bash

#Log stdout and stderr to file
TMP_XSTUDIO_BUILD_TIME=$(date +%Y%m%d%H%M%S)
TMP_XSTUDIO_BUILD_LOG=xstudiobuild-${TMP_XSTUDIO_BUILD_TIME}.log
exec >  >(tee -ia ${TMP_XSTUDIO_BUILD_LOG})
exec 2> >(tee -ia ${TMP_XSTUDIO_BUILD_LOG} >&2)

## Rocky Linux 9.x
#[Download](https://rockylinux.org/download "Download")

MAKE_JOBS=8
TMP_XSTUDIO_BUILD_DIR=${HOME}/tmp_build_xstudio
VER_XSTUDIO=main

VER_ACTOR=0.18.4
# VER_ACTOR=0.19.6
# VER_ACTOR=1.0.2

VER_AUTOCONF=2.72

VER_FDK_AAC=latest

VER_FFMPEG=5.1.6
# VER_FFMPEG=6.1.2

VER_FMTLIB=8.0.1
# VER_FMTLIB=11.0.2

VER_libGLEW=2.1.0
#VER_libGLEW=2.2.0

VER_NASM=2.16.03

VER_NLOHMANN=3.11.2
#VER_NLOHMANN=3.11.3

VER_OCIO2=2.2.1
#VER_OCIO2=2.4.0

VER_OPENEXR=RB-3.3

VER_OpenTimelineIO=0.17.0

VER_PYTHON=3.9

VER_SPDLOG=1.9.2
# VER_SPDLOG=1.15.0

VER_x264=stable

VER_x265=4.0

VER_YASM=1.3.0

mkdir -p ${TMP_XSTUDIO_BUILD_DIR}

### Distro installs
sudo dnf config-manager --set-enabled crb
sudo dnf update -y
sudo dnf groupinstall "Development Tools" -y
sudo dnf install wget git cmake python-devel pybind11-devel -y
sudo dnf install alsa-lib-devel pulseaudio-libs-devel -y
sudo dnf install freeglut-devel libjpeg-devel libuuid-devel -y
sudo dnf install doxygen python3-sphinx -y
sudo dnf install opus-devel libvpx-devel openjpeg2-devel lame-devel -y
sudo dnf install qt5 qt5-devel -y
sudo dnf install libXmu-devel libXi-devel libGL-devel -y
pip install --user sphinx_rtd_theme breathe

#### Qt6
# sudo dnf install epel-release -y
# sudo dnf install qt6-qtbase-devel -y

### Local installs

#### pybind11
# cd ${TMP_XSTUDIO_BUILD_DIR}
# git clone https://github.com/pybind/pybind11.git
# cd pybind11
# sudo python setup.py install
# cd ${TMP_XSTUDIO_BUILD_DIR}

#### libGLEW
cd ${TMP_XSTUDIO_BUILD_DIR}
wget https://github.com/nigels-com/glew/releases/download/glew-${VER_libGLEW}/glew-${VER_libGLEW}.tgz
tar -xf glew-${VER_libGLEW}.tgz
cd glew-${VER_libGLEW}
make -j${MAKE_JOBS}
sudo make install
cd ${TMP_XSTUDIO_BUILD_DIR}

#### NLOHMANN JSON
cd ${TMP_XSTUDIO_BUILD_DIR}
wget -O json-v${VER_NLOHMANN}.tar.gz https://github.com/nlohmann/json/archive/refs/tags/v${VER_NLOHMANN}.tar.gz
tar -xf json-v${VER_NLOHMANN}.tar.gz
mkdir json-${VER_NLOHMANN}/build
cd json-${VER_NLOHMANN}/build
cmake .. -DJSON_BuildTests=Off
make -j${MAKE_JOBS}
sudo make install
cd ${TMP_XSTUDIO_BUILD_DIR}

#### OpenEXR
cd ${TMP_XSTUDIO_BUILD_DIR}
git clone https://github.com/AcademySoftwareFoundation/openexr.git
cd openexr/
git checkout ${VER_OPENEXR}
mkdir build
cd build
cmake .. -DOPENEXR_INSTALL_TOOLS=Off -DBUILD_TESTING=Off
make -j${MAKE_JOBS}
sudo make install
cd ${TMP_XSTUDIO_BUILD_DIR}

#### ActorFramework
cd ${TMP_XSTUDIO_BUILD_DIR}
wget -O actor-${VER_ACTOR}.tar.gz https://github.com/actor-framework/actor-framework/archive/refs/tags/${VER_ACTOR}.tar.gz
tar -xf actor-${VER_ACTOR}.tar.gz
cd actor-framework-${VER_ACTOR}
./configure
cd build
make -j${MAKE_JOBS}
sudo make install
cd ${TMP_XSTUDIO_BUILD_DIR}

#### OCIO2
cd ${TMP_XSTUDIO_BUILD_DIR}
wget -O ocio-v${VER_OCIO2}.tar.gz https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/refs/tags/v${VER_OCIO2}.tar.gz
tar -xf ocio-v${VER_OCIO2}.tar.gz
cd OpenColorIO-${VER_OCIO2}/
mkdir build
cd build
cmake -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_TESTS=OFF -DOCIO_BUILD_GPU_TESTS=OFF ../
make -j${MAKE_JOBS}
sudo make install
cd ${TMP_XSTUDIO_BUILD_DIR}

#### SPDLOG
cd ${TMP_XSTUDIO_BUILD_DIR}
wget -O spd-v${VER_SPDLOG}.tar.gz https://github.com/gabime/spdlog/archive/refs/tags/v${VER_SPDLOG}.tar.gz
tar -xf spd-v${VER_SPDLOG}.tar.gz
cd spdlog-${VER_SPDLOG}
mkdir build
cd build
cmake .. -DSPDLOG_BUILD_SHARED=On
make -j${MAKE_JOBS}
sudo make install
cd ${TMP_XSTUDIO_BUILD_DIR}

#### FMTLIB
cd ${TMP_XSTUDIO_BUILD_DIR}
wget -O fmt-${VER_FMTLIB}.tar.gz https://github.com/fmtlib/fmt/archive/refs/tags/${VER_FMTLIB}.tar.gz
tar -xf fmt-${VER_FMTLIB}.tar.gz
cd fmt-${VER_FMTLIB}/
mkdir build
cd build
cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=1 -DFMT_DOC=Off -DFMT_TEST=Off
make -j${MAKE_JOBS}
sudo make install
cd ${TMP_XSTUDIO_BUILD_DIR}

#### OpenTimelineIO
cd ${TMP_XSTUDIO_BUILD_DIR}
git clone https://github.com/AcademySoftwareFoundation/OpenTimelineIO.git
cd OpenTimelineIO
git checkout tags/v${VER_OpenTimelineIO} -b v${VER_OpenTimelineIO}
mkdir build
cd build
cmake -DOTIO_PYTHON_INSTALL=ON -DOTIO_DEPENDENCIES_INSTALL=OFF -DOTIO_FIND_IMATH=ON ..
make -j${MAKE_JOBS}
sudo make install
cd ${TMP_XSTUDIO_BUILD_DIR}

#### FFMPEG AND DEPS

##### Autoconf
cd ${TMP_XSTUDIO_BUILD_DIR}
wget https://mirror.us-midwest-1.nexcess.net/gnu/autoconf/autoconf-${VER_AUTOCONF}.tar.gz
tar -xf autoconf-${VER_AUTOCONF}.tar.gz
cd autoconf-${VER_AUTOCONF}
./configure
make -j${MAKE_JOBS}
sudo make install
cd ${TMP_XSTUDIO_BUILD_DIR}

##### NASM
cd ${TMP_XSTUDIO_BUILD_DIR}
git clone https://github.com/netwide-assembler/nasm.git
cd nasm
git checkout tags/nasm-${VER_NASM} -b nasm-${VER_NASM}
./autogen.sh
./configure
make -j${MAKE_JOBS}
sudo make install
cd ${TMP_XSTUDIO_BUILD_DIR}

# ##### YASM
# cd ${TMP_XSTUDIO_BUILD_DIR}
# git clone https://github.com/yasm/yasm.git
# cd yasm
# git checkout tags/v${VER_YASM} -b v${VER_YASM}
# ./configure --prefix="$HOME/ffmpeg_build" --bindir="$HOME/bin"
# make -j  $JOBS
# sudo make install
# cd ${TMP_XSTUDIO_BUILD_DIR}
sudo dnf --enablerepo=devel install yasm yasm-devel -y

##### x264
cd ${TMP_XSTUDIO_BUILD_DIR}
git clone --branch ${VER_x264} --depth 1 https://code.videolan.org/videolan/x264.git
cd x264/
./configure --enable-shared
make -j${MAKE_JOBS}
sudo make install
cd ${TMP_XSTUDIO_BUILD_DIR}

##### x265
cd ${TMP_XSTUDIO_BUILD_DIR}
wget https://bitbucket.org/multicoreware/x265_git/downloads/x265_${VER_x265}.tar.gz
tar -xf x265_${VER_x265}.tar.gz
cd x265_${VER_x265}/build/linux/
cmake -G "Unix Makefiles" ../../source
make -j${MAKE_JOBS}
sudo make install
cd ${TMP_XSTUDIO_BUILD_DIR}

##### FDK-AAC
cd ${TMP_XSTUDIO_BUILD_DIR}
git clone --depth 1 https://github.com/mstorsjo/fdk-aac
cd fdk-aac
autoreconf -fiv
./configure
make -j${MAKE_JOBS}
sudo make install
cd ${TMP_XSTUDIO_BUILD_DIR}

##### FFMPEG
cd ${TMP_XSTUDIO_BUILD_DIR}
wget https://ffmpeg.org/releases/ffmpeg-${VER_FFMPEG}.tar.bz2
tar -xf ffmpeg-${VER_FFMPEG}.tar.bz2
cd ffmpeg-${VER_FFMPEG}/
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
./configure --extra-libs=-lpthread --extra-libs=-lm --enable-gpl \
    --enable-libfdk_aac --enable-libfreetype --enable-libmp3lame \
    --enable-libopus --enable-libvpx --enable-libx264 --enable-libx265 \
    --enable-shared --enable-nonfree
make -j${MAKE_JOBS}
sudo make install
cd ${TMP_XSTUDIO_BUILD_DIR}

##### xStudio
cd ${TMP_XSTUDIO_BUILD_DIR}
git clone https://github.com/AcademySoftwareFoundation/xstudio.git
cd xstudio
git checkout ${VER_XSTUDIO}
export PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:/usr/local/lib64/pkgconfig
mkdir build
cd build
cmake .. -DBUILD_DOCS=Off
make -j${MAKE_JOBS}
cd ${TMP_XSTUDIO_BUILD_DIR}

##### Ensure /usr/local/lib is in ldconf
cat << EOF | sudo tee /etc/ld.so.conf.d/usr-local-lib.conf
/usr/local/lib
EOF
sudo ldconfig

##### Create launch shortcut
cd ${TMP_XSTUDIO_BUILD_DIR}
cat << EOF > start_xstudio.sh
sudo setenforce 0
export QV4_FORCE_INTERPRETER=1
export LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64
export PYTHONPATH=./bin/python/lib/python${VER_PYTHON}/site-packages:/home/xstudio/.local/lib/python${VER_PYTHON}/site-packages:

cd ${TMP_XSTUDIO_BUILD_DIR}/xstudio/build
./bin/xstudio.bin
EOF
chmod +x start_xstudio.sh
