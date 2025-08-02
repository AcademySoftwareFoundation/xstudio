#!/bin/bash

docker run --network=host --volume=echo ~:/home/${USER} --user=id -u ${USER} --env="DISPLAY" --volume="/etc/passwd:/etc/passwd:ro" dneg/xstudio

