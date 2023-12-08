## Ubuntu 22.04 LTS
[Download](https://releases.ubuntu.com/22.04 "Download")

### Distro installs
    sudo apt install build-essential cmake git python3-pip
    sudo apt install doxygen sphinx-common sphinx-rtd-theme-common python3-breathe
    sudo apt install python-is-python3 pybind11-dev libpython3-dev
    sudo apt install libspdlog-dev libfmt-dev libssl-dev zlib1g-dev libasound2-dev nlohmann-json3-dev uuid-dev
    sudo apt install libglu1-mesa-dev freeglut3-dev mesa-common-dev libglew-dev libfreetype-dev
    sudo apt install libjpeg-dev libpulse-dev nlohmann-json3-dev
    sudo apt install yasm nasm libfdk-aac-dev libfdk-aac2 libmp3lame-dev libopus-dev libvpx-dev libx265-dev libx264-dev
    sudo apt install  qttools5-dev qtbase5-dev qt5-qmake  qtdeclarative5-dev qtquickcontrols2-5-dev
    sudo apt install qml-module-qtquick* qml-module-qt-labs-*

    pip install sphinx_rtd_theme


### Local installs
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


#### FFMPEG
    wget https://ffmpeg.org/releases/ffmpeg-5.1.tar.bz2
    tar -xf ffmpeg-5.1.tar.bz2
    cd ffmpeg-5.1/
    export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
    ./configure --extra-libs=-lpthread   --extra-libs=-lm    --enable-gpl   --enable-libfdk_aac   --enable-libfreetype   --enable-libmp3lame   --enable-libopus   --enable-libvpx   --enable-libx264   --enable-libx265 --enable-shared --enable-nonfree
    make -j  $JOBS
    sudo make install
    cd -


### xStudio
    export PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:/usr/local/lib64/pkgconfig
    cd  xstudio
    mkdir build
    cd build
    cmake .. -DBUILD_DOCS=Off
    make -j $JOBS

    export QV4_FORCE_INTERPRETER=1
    export LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64
    export PYTHONPATH=./bin/python/lib/python3.10/site-packages:/home/xstudio/.local/lib/python3.10/site-packages:

    ./bin/xstudio.bin
