cosy-fmri-camera-recorder
=========================

Linux driver
------------

dmesg output:
[ 4165.009790] usb 2-1.8: new high-speed USB device number 5 using ehci-pci
[ 4165.092571] usb 2-1.8: config 1 interface 0 altsetting 1 bulk endpoint 0x83 has invalid maxpacket 256
[ 4165.095881] usb 2-1.8: New USB device found, idVendor=1b71, idProduct=3002
[ 4165.095885] usb 2-1.8: New USB device strings: Mfr=3, Product=4, SerialNumber=2
[ 4165.095887] usb 2-1.8: Product: usbtv007
[ 4165.095890] usb 2-1.8: Manufacturer: fushicai
[ 4165.095892] usb 2-1.8: SerialNumber: 300000000002
[ 4166.353720] media: Linux media interface: v0.10
[ 4166.369079] Linux video capture interface: v2.00
[ 4166.429189] usbtv 2-1.8:1.0: Fushicai USBTV007 Audio-Video Grabber
[ 4166.429253] usbcore: registered new interface driver usbtv

Uses the usbtv kernel driver. Available since Linux 3.11. The source code in
the Linux kernel is available in the drivers/media/usb/usbtv/ directory.

Initial commit that added the usbtv driver:
https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=f3d27f34fdd7701e499617d2c1d94480a98f6d07

$ git tag --contains f3d27f34fdd7701e499617d2c1d94480a98f6d07 | head -1
v3.11

CentOS 7 uses Linux 3.10, so the kernel driver is not available there.
A more recent kernel is needed, for example with Fedora.
