# CheaChinVR
(WIP) Cheap Chinese Video Recorder companion software.

This project aims to replace the CMS and VMS software provided with cheap chinese DVRs and NVRs. The original software is closed-source, requires admin privileges, runs only on Windows and is difficult to use. Our mission is to fix that.

There are already open-source multi-platform tools related to the DVRs and NVRs that have dissected most of the protocol used (Sofia / NETsurveillance), but those provide no easy-to-use GUI and are not meant as a replacement of the manufacturer's software.

# Milestones
- [X] Research relevant technologies, other projects, potential libraries
- [X] Make a simple simulator to test the protocol against
- [X] Make a console app that can connect and login to a real NVR system
- [ ] Make a console app that will capture a part of video stream from a real NVR system into a file
- [ ] Make a GUI app that can connect and login to a real NVR system
- [ ] Add video display from the NVR (only current video, no history)
- [ ] Add history search, playback and export
- [ ] Allow editing NVR settings
- [ ] Add a daemon to store alerts from the NVR; browse history using these alerts

For testing purposes, I will be mainly using a NBD8016S-XC board (available for cheap from AliExpress).

# Resources, relevant other projects, potential libraries
- [Python-dvr](https://github.com/madmaxoft/python-dvr) - Python library for talking the Sofia protocol.
- [SofiaCtl](https://github.com/madmaxoft/sofiactl) - Perl library for talking the Sofia protocol.
- [IPCTimeLapse](https://github.com/charmyin/IPCTimeLapse) - Set of shell scripts and simple programs talking the Sofia protocol.
- [libVLC](https://github.com/videolan/libvlcpp) - C++ wrappers for the VLC library providing media playback.
- [Custom video streaming using Qt and VLC](http://derekmolloy.ie/custom-video-streaming-player-using-libvlc-and-qt/) - Blog describing integrating streaming video playback into a custom app built in Qt
- [wxVLC](https://github.com/tomay3000/wxVLC) - wxWidgets- and VLC-based media player. May provide pointers for integrating VLC playback with wxWidgets.
