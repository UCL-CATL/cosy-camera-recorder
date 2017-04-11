cosy-camera-recorder
====================

Recording a video coming from a camera or webcam, based on the
[GStreamer](https://gstreamer.freedesktop.org/) library. The recording is
started and stopped with [ZeroMQ](http://zeromq.org/) requests.

This program has been developed on GNU/Linux.

cosy-camera-recorder is licensed under the GNU General Public License version 3
or later. See the file `COPYING` for more information.

Use-case
--------

Ideally we wanted to use the [Pupil Capture](https://pupil-labs.com/)
application, but it doesn't support the camera that we want to use. It's an
MR-compatible camera. To connect it to the PC, a video grabber must be used. We
have tried two different video grabbers:

- Fushicai USBTV007 Audio-Video Grabber (usbtv Linux driver)
- USB2.0 PC CAMERA (UVC device)

Pupil Capture currently supports only
[UVC](https://en.wikipedia.org/wiki/USB_video_device_class) devices, but the
second video grabber doesn't work well in Pupil Capture, there are lots of
errors about corrupt JPEG data.

GStreamer supports well all camera devices that we have tested: the above two
video grabbers, and the default webcams that come with the Pupil headset.

Ideally a new video capture backend based on GStreamer should be developed for
Pupil Capture. The program in this repository is a much simpler solution, for
the cases where the eye tracking data don't need to be available in real-time.
The recorded video can then be opened in Pupil Capture to gather the pupil
information (mainly the pupil diameter).

**Update:** finally the UVC video grabber works well with Pupil Capture
(version 0.9.3 at least). So we will use Pupil Capture instead of this program.
At least writing this program has permitted me to learn GStreamer.
