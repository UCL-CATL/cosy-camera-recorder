cosy-camera-recorder
====================

Recording a video coming from a camera or webcam, based on the GStreamer
library. The recording is started and stopped with ZeroMQ requests.

Use-case
--------

Ideally we wanted to use the Pupil Capture application, but it doesn't support
the camera that we want to use. It's an MR-compatible camera. To connect it to
the PC, a video grabber must be used. We have tried two different video
grabbers:

- Fushicai USBTV007 Audio-Video Grabber (usbtv Linux driver)
- USB2.0 PC CAMERA (UVC device)

Pupil Capture currently supports only UVC devices, but the second video grabber
doesn't work well in Pupil Capture, there are lots of errors about corrupt JPEG
data.

GStreamer supports well all camera devices that we have tested: the above two
video grabbers, and the default webcams that come with the Pupil headset.

Ideally a new video capture backend based on GStreamer should be developed for
Pupil Capture. The program in this repository is a much simpler solution, for
the cases where the eye tracking data don't need to be available in real-time.
