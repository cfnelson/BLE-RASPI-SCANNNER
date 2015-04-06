# mustached-octo-bugfixes
TO COMPILE:

gcc -o ble_raspi_scanner ble_raspi_scanner.c -lbluetooth

TO RUN AND SEND TO A SERVER:

sudo python python_ble_scanner.py

NOTES:

you must have the libbluetooth-dev installed on your system. 

if you want to change the server it is posting too then you must edit the python code and change the server and the port. 
