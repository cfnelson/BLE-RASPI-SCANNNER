# BLE RASPI SCANNNER
This project is Open-Source under the GPL-2.0 Licence.

The project has been written in both C and Python.  

The project was developed to test and demonstrate "one" possible approach to a light-weight program for capturing Bluetooth Low Energy (BLE) Advertising Packets, specifically iBeacon Packets on a RaspberryPi (Raspi).  

The captured iBeacon Packets would be immediately forwarded onto a node.js webserver.  

This particular implementation uses websockets to forward the captured packets to a socket.io/node.js webserver. 

##Hardware & Software Used:
* 1 x RaspberryPi B+,
* 1 x Bluetooth 4.0  USB Dongle,
* 1 x Wifi USB Dongle or Ethernet Cable,
* OS: Raspbian Debian Wheezy,
* Bluez - Official Linux Bluetooth protocol stack.

Note: Ensure that your linux distro has the latest `libbluetooth-dev` installed.

##Before Running:
* Ensure that your linux distro has all the required dependencies.
* Ensure that you have specified the intended server and port correctly in the `python_ble_scanner.py` file.
* You must edit the `base_url` variable to match your intended server and port.
* The webserver must be online and ready to accept a connection. 

####TO COMPILE:
Enter the following command into your console:

```gcc -o ble_raspi_scanner ble_raspi_scanner.c -lbluetooth```

####TO RUN:
Enter the following command into your console: 

```sudo python python_ble_scanner.py```
