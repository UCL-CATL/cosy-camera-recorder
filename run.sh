#!/usr/bin/env bash

if [ "$#" -lt "1" ]; then
	recordings_dir=~/pupil/cosy-camera-recorder-videos
else
	recordings_dir=$1
fi

docker run -it --rm \
	--privileged \
	--net=host \
	--volume $recordings_dir:/root/cosy-camera-recorder/src/cosy-camera-recorder-videos \
	--volume /etc/localtime:/etc/localtime:ro \
	ucl-cosy/cosy-camera-recorder
