#!/bin/bash

# see: https://stackoverflow.com/questions/16296753/can-you-run-gui-applications-in-a-linux-docker-container
# to get display from docker container to host


XSOCK=/tmp/.X11-unix/X50
XAUTH=/tmp/.docker.xauth

xauth nlist $DISPLAY | sed -e 's/^..../ffff/' | xauth -f $XAUTH nmerge -
export QT_GRAPHICSSYSTEM=native
export QT_X11_NO_MITSHM=1



docker run -it -v $XSOCK:$XSOCK -v /home/ling/.Xauthority:/root/.Xauthority:ro -e XAUTHORIY=/root/.Xauthority -e DISPLAY=$DISPLAY --net=host dneg/xstudio bash
