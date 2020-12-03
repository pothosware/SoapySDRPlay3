# Soapy SDR module for SDRPlay

## Documentation

* https://github.com/pothosware/SoapySDRPlay/wiki

## Dependencies

* Get SDR Play driver binaries 'API/HW driver V3.x' from - http://sdrplay.com/downloads
* SoapySDR - https://github.com/pothosware/SoapySDR/wiki

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

