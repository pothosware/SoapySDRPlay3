# Soapy SDR module for SDRPlay

> **Note:** This module contains AI-generated code.

## Documentation

* https://github.com/pothosware/SoapySDRPlay/wiki

## Dependencies

* SDRplay API - download (and install) SDRplay API from - https://www.sdrplay.com/downloads - NOTE: the current version of this module requires SDRplay API V3.15 or later
* SoapySDR - https://github.com/pothosware/SoapySDR/wiki

## RSPduo Dual Tuner mode

Open the device with `mode=DT` to run both tuners at once:

```
SoapySDRUtil --probe="driver=sdrplay,serial=<serial>,mode=DT"
```

The device then has two receive channels, and both of them can be carried by
one stream:

```c++
device->setupStream(SOAPY_SDR_RX, "CF32", {0, 1});
```

`readStream()` fills one buffer per channel and the two are sample aligned: the
tuners deliver their samples in matched pairs and the driver keeps the pairing
all the way to the caller's buffers, which is what diversity reception needs.
Setting the two channels up as two separate streams still works and is still
what a single tuner mode gives you, but two streams are read one after the
other and nothing keeps them in step.

Channel 0 is tuner A ("Tuner 1 50 ohm") and channel 1 is tuner B ("Tuner 2 50
ohm"); neither can be swapped or moved to the Hi-Z port while both are running.
The gain, the AGC, the bandwidth, the DC/IQ correction and the tuner settings
(`biasT_ctrl`, `rfnotch_ctrl`, `dabnotch_ctrl`, `agc_setpoint`, `iqcorr_ctrl`)
are each channel's own, through the per channel `writeSetting()`. The device
wide `writeSetting()` writes both channels, so an application that knows
nothing of the second tuner still configures the pair the same way.

The sample rate is the converter's rather than a tuner's, so it moves both
channels together. Dual Tuner mode runs at a low IF and offers 62.5 kHz to
2 MHz.

## Troubleshooting

This section contains some useful information for troubleshhoting

##### Message: `[WARNING] Can't find label in args`

An error message like this one:
```
Probe device driver=sdrplay
[WARNING] Can't find label in args
Error probing device: Can't find label in args
```

could be due to the OS not being able to 'see' the RSP as a USB device.

You may want to check using the command `lsusb`:
```
lsusb -d 1df7:
```
The output should look similar to this:
```
Bus 002 Device 006: ID 1df7:3010
```
If the `lsusb` command above returns nothing, it means the OS is not able to see the RSP (which could be due to a moltitude of reasons, like problems with the OS, bad USB cable, bad hardware, etc).

Another way to verify that the OS is able to see the RSP device is by running the `dmesg` command
```
dmesg
```
and look for lines similar to these (the idVendor value should be 1df7):
```
[ 1368.128506] usb 2-2: new high-speed USB device number 6 using xhci_hcd
[ 1368.255007] usb 2-2: New USB device found, idVendor=1df7, idProduct=3010, bcdDevice= 2.00
[ 1368.255016] usb 2-2: New USB device strings: Mfr=0, Product=0, SerialNumber=0
```

If there's nothing like that, try first to disconnect the RSP and then connect it back; if that does not work, try rebooting the computer; if that does not work either, try the RSP on a different computer with a different USB cable.


## Licensing information

The MIT License (MIT)

Copyright (c) 2015 Charles J. Cliffe<br/>
Copyright (c) 2020 Franco Venturi - changes for SDRplay API version 3 and Dual Tuner for RSPduo


Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

