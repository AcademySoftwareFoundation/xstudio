
## Centos 7
[Download](https://www.centos.org/download/ "Download")
**Default Mesa drivers will not work, as they are to old, NVidia/AMD/Intel custom drivers should be fine.
**
### Distro installs
    sudo yum install -y centos-release-scl
    sudo yum install -y devtoolset-9
    sudo yum install -y alsa-lib-devel pulseaudio-libs-devel freeglut-devel
    sudo yum install -y python3-devel bzip2-devel freetype-devel zlib-devel
    sudo yum install -y libuuid-devel

    #### Qt 5.15
Install 5.15 dev tools, using Qt5 online installer, requires login account (free).
*Or if you are brave or crazy, build from source.*


    #### Force use of newer GCC
    sudo -s
    echo "source /opt/rh/devtoolset-9/enable" >> /etc/bashrc

    *(New Shell to pick up change)*


### Local installs

#### OPENSSL
    wget https://github.com/openssl/openssl/archive/refs/tags/OpenSSL_1_1_1s.tar.gz
    tar -xf OpenSSL_1_1_1s.tar.gz
    cd openssl-OpenSSL_1_1_1s/
    ./config
    make -j $JOBS
    sudo make install
    cd -

#### CMake
    wget https://github.com/Kitware/CMake/releases/download/v3.25.1/cmake-3.25.1.tar.gz
    tar -xf cmake-3.25.1.tar.gz
    cd cmake-3.25.1/
    ./configure
    gmake -j $JOBS
    sudo gmake install
    cd -

#### NLOHMANN JSON
    wget https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.tar.gz
    tar -xf v3.11.2.tar.gz
    mkdir json-3.11.2/build
    cd json-3.11.2/build
    cmake .. -DJSON_BuildTests=Off
    make -j $JOBS
    sudo make  install
    cd -


#### PYBIND11
    wget https://github.com/pybind/pybind11/archive/refs/tags/v2.6.2.tar.gz
    tar -xf v2.6.2.tar.gz
    cd pybind11-2.6.2
    mkdir build
    cd build
    cmake ..
    make -j $JOBS
    sudo make install
    cd ../..

#### SPDLOG
    wget https://github.com/gabime/spdlog/archive/refs/tags/v1.9.2.tar.gz
    tar -xf v1.9.2.tar.gz
    cd spdlog-1.9.2
    mkdir build
    cd build
    cmake .. -DSPDLOG_BUILD_SHARED=On
    make -j $JOBS
    sudo make install
    cd ../..

#### FMTLIB
    wget https://github.com/fmtlib/fmt/archive/refs/tags/8.0.1.tar.gz
    tar -xf 8.0.1.tar.gz
    cd fmt-8.0.1/
    mkdir build
    cd build
    cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=1 -DFMT_DOC=Off -DFMT_TEST=Off
    make -j $JOBS
    sudo make install
    cd ../..

#### OpenEXR

    git clone https://github.com/AcademySoftwareFoundation/openexr.git
    cd openexr/
    git checkout RB-3.1
    mkdir build
    cd build
    cmake .. -DOPENEXR_INSTALL_TOOLS=Off -DBUILD_TESTING=Off
    make -j $JOBS
    sudo make install
    cd ../..

#### ActorFramework
    wget https://github.com/actor-framework/actor-framework/archive/refs/tags/0.18.4.tar.gz
    tar -xf 0.18.4.tar.gz
    cd actor-framework-0.18.4
    ./configure
    cd build
    make -j $JOBS
    sudo make install
    cd ../..

#### OCIO2
    wget https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/refs/tags/v2.2.0.tar.gz
    tar -xf v2.2.0.tar.gz
    cd OpenColorIO-2.2.0/
    mkdir build
    cd build
    cmake -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_TESTS=OFF -DOCIO_BUILD_GPU_TESTS=OFF ../
    make -j $JOBS
    sudo make install
    cd ../..


#### libGLEW
    wget https://sourceforge.net/projects/glew/files/glew/2.1.0/glew-2.1.0.tgz
    tar -xf glew-2.1.0.tgz
    cd glew-2.1.0/
    make -j $JOBS
    sudo make install
    cd -


#### OpenTimelineIO
    git clone https://github.com/AcademySoftwareFoundation/OpenTimelineIO.git
    cd OpenTimelineIO
    git checkout cxx17
    mkdir build
    cd build
    cmake -DOTIO_PYTHON_INSTALL=ON -DOTIO_DEPENDENCIES_INSTALL=OFF -DOTIO_FIND_IMATH=ON ..
    make -j $JOBS
    sudo make install
    cd ../..


#### FFMPEG AND DEPS

##### NASM
    wget https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/nasm-2.15.05.tar.bz2
    tar -xf nasm-2.15.05.tar.bz2
    cd nasm-2.15.05
    ./autogen.sh
    ./configure
    make -j $JOBS
    sudo make install
    cd -

##### YASM
    wget https://www.tortall.net/projects/yasm/releases/yasm-1.3.0.tar.gz
    tar -xf yasm-1.3.0.tar.gz
    cd yasm-1.3.0
    ./configure --prefix="$HOME/ffmpeg_build" --bindir="$HOME/bin"
    make -j $JOBS
    sudo make install
    cd -

##### TurboJPEG
    wget https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/2.1.4.tar.gz
    tar -xf 2.1.4.tar.gz
    cd libjpeg-turbo-2.1.4
    mkdir build
    cd build/
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
    make -j $JOBS
    sudo make install
    cd ../..

##### x264
    git clone --branch stable --depth 1 https://code.videolan.org/videolan/x264.git
    cd x264/
    ./configure --enable-shared
    make -j $JOBS
    sudo make install
    cd -

##### x265
    wget https://bitbucket.org/multicoreware/x265_git/downloads/x265_3.5.tar.gz
    tar -xf x265_3.5.tar.gz
    cd x265_3.5/build/linux/
    cmake -G "Unix Makefiles" ../../source
    make -j $JOBS
    sudo make install
    cd -

##### LAME
    wget https://downloads.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz
    tar -xf lame-3.100.tar.gz
    cd lame-3.100
    ./configure --enable-nasm
    make -j $JOBS
    sudo make install
    cd -

##### FDK-AAC
    git clone --depth 1 https://github.com/mstorsjo/fdk-aac
    cd fdk-aac
    autoreconf -fiv
    ./configure
    make -j $JOBS
    sudo make install
    cd -

##### OPUS
    wget https://archive.mozilla.org/pub/opus/opus-1.3.1.tar.gz
    tar xzvf opus-1.3.1.tar.gz
    cd opus-1.3.1
    ./configure
    make -j $JOBS
    sudo make install
    cd -

##### LIBVPX
    git clone --depth 1 https://chromium.googlesource.com/webm/libvpx.git
    cd libvpx
    ./configure --enable-shared --disable-static --disable-examples --disable-unit-tests --enable-vp9-highbitdepth --as=yasm
    make -j $JOBS
    sudo make install
    cd -

##### FFMPEG
    wget https://ffmpeg.org/releases/ffmpeg-5.1.tar.bz2
    tar -xf ffmpeg-5.1.tar.bz2
    cd ffmpeg-5.1/
    export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
    ./configure --extra-libs=-lpthread   --extra-libs=-lm    --enable-gpl   --enable-libfdk_aac   --enable-libfreetype   --enable-libmp3lame   --enable-libopus   --enable-libvpx   --enable-libx264   --enable-libx265 --enable-shared --enable-nonfree
    make -j $JOBS
    sudo make install
    cd -

### xStudio
    export PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:/usr/local/lib64/pkgconfig
    export Qt5_DIR=/opt/Qt/5.15.2/gcc_64
    export LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64
    export PYTHONPATH=./bin/python/lib/python3.6/site-packages:/home/xstudio/.local/lib/python3.6/site-packages:

    cd  xstudio
    mkdir build
    cd build
    cmake .. -DBUILD_DOCS=Off
    make -j $JOBS

    ./bin/xstudio.bin