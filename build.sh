#!/usr/bin/env bash

git clean -Xdf
docker build --rm --tag ucl-cosy/cosy-camera-recorder:0.4 .
