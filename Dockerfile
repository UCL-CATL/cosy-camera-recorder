FROM fedora:24
MAINTAINER Sébastien Wilmet

RUN dnf -y upgrade && \
	dnf -y group install "C Development Tools and Libraries" && \
	dnf clean all

RUN dnf -y install https://raw.githubusercontent.com/UnitedRPMs/unitedrpms/master/RPM/unitedrpms-24-2.noarch.rpm && \
	rpm --import https://raw.githubusercontent.com/UnitedRPMs/unitedrpms.github.io/master/URPMS-GPG-PUBLICKEY-Fedora-24 && \
	dnf -y upgrade && \
	dnf -y install gstreamer1-{ffmpeg,libav,plugins-{good,ugly,bad{,-free,-nonfree}}} --setopt=strict=0 && \
	dnf -y install \
		zeromq-devel \
		czmq-devel \
		glib2-devel \
		git \
		gstreamer1-devel \
		gstreamer1-plugins-base-devel \
		gstreamer1-plugins-bad-free-devel && \
	dnf clean all

ADD . /root/cosy-camera-recorder

# Make sure that the code is compilable
RUN cd /root/cosy-camera-recorder && \
	cd src && make clean && make && cd .. && \
	cd tests && make clean && make

WORKDIR /root/cosy-camera-recorder/src

# Set default command
CMD ["/usr/bin/bash"]
