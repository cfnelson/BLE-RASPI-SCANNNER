# mustached-octo-bugfixes
TO COMPILE:

gcc -o ble_raspi_scanner ble_raspi_scanner.c -lbluetooth

TO RUN AND SEND TO A SERVER:

sudo python python_ble_scanner.py

NOTES - Read before running:

Python code is a simple example/demo of using websockets to send ibeacon packets to a nodejs/socket.io server.

You must have the libbluetooth-dev installed on your system. 

You must edit the python code by changing the base_url variable to match to your intended server and port. 
Read the inline comments, for more details. 
