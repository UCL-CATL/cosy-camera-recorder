#!/usr/bin/env bash

if [ "$#" -lt "1" ]; then
	recordings_dir=~/pupil/cosy-camera-recorder-videos
else
	recordings_dir=$1
fi

# Needed for a GUI application.
XSOCK=/tmp/.X11-unix
XAUTH=/tmp/.docker.xauth
> $XAUTH
xauth nlist :0 | sed -e 's/^..../ffff/' | xauth -f $XAUTH nmerge -

docker run -it --rm \
	--privileged \
	--net=host \
	--env DISPLAY=$DISPLAY \
	--volume $XSOCK:$XSOCK \
	--volume $XAUTH:$XAUTH --env XAUTHORITY=$XAUTH \
	--volume $recordings_dir:/root/cosy-camera-recorder/src/cosy-camera-recorder-videos \
	--volume /etc/localtime:/etc/localtime:ro \
	ucl-cosy/cosy-camera-recorder:0.3.1
