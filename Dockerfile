# https://github.com/UCL-CATL/cosy-docker-layer
FROM ucl-cosy/cosy-docker-layer:25

MAINTAINER Sébastien Wilmet

RUN dnf -y install gstreamer1-{ffmpeg,libav,plugins-{good,ugly,bad{,-free}}} --setopt=strict=0 && \
	dnf -y install \
		autoconf-archive \
		cheese \
		zeromq-devel \
		czmq-devel && \
	dnf -y builddep cheese-libs && \
	dnf clean all

ADD . /root/cosy-camera-recorder

# Make sure that the code is compilable
RUN cd /root/cosy-camera-recorder && \
	./autogen.sh && make

WORKDIR /root/cosy-camera-recorder/src

# Set default command
CMD ["/usr/bin/bash"]
