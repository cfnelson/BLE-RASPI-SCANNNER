/*
    ble_raspi_scanner - Bluetooth low energy scanning of low energy advertisements, captures iBeacon packets and displays them.
   	Copyright (C) 2014  Charles Nelson
	
   	************************************NOTE***********************************
    Code is based/re-factored from and uses the Bluez - Bluetooth protocol stack for Linux. 
    
    You can find out more at - http://www.bluez.org/
    
    Contributors of the original source code used are: 
      Copyright (C) 2012  Intel Corporation. All rights reserved. 	
      Copyright (C) 2000-2001  Qualcomm Incorporated
      Copyright (C) 2002-2003  Maxim Krasnyansky <maxk@qualcomm.com>
      Copyright (C) 2002-2010  Marcel Holtmann <marcel@holtmann.org>
    ***************************************************************************
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <signal.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

static struct hci_dev_info di;                                  //- Stores our ble adapter's device info e.g- our address to use as a handle
static volatile int signal_received = 0;

static void sigint_handler(int sig)
{
	signal_received = sig;
}

static int print_advertising_ble_beacons(int dd, uint8_t filter_type)
{
  struct hci_filter nf, of;
  struct sigaction sa;
  socklen_t olen;
  int len, i;
  char hostname[128];            				//- Used to retrieve the host name
  char buff[100];						//- Used for the time-stamp
  unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;

  olen = sizeof(of);
  if (getsockopt(dd, SOL_HCI, HCI_FILTER, &of, &olen) < 0)
  {
      printf("Could not get socket options\n");
      return -1;
  }

  hci_filter_clear(&nf);
  hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
  hci_filter_set_event(EVT_LE_META_EVENT, &nf);

  if (setsockopt(dd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0)
  {
      printf("Could not set socket options\n");
      return -1;
  }
  memset(&sa, 0, sizeof(sa));
  sa.sa_flags     = SA_NOCLDSTOP;
  sa.sa_handler   = sigint_handler;
  sigaction(SIGINT, &sa, NULL);

  gethostname(hostname, sizeof hostname);         										//- Retrieving the hostname of the device that is scanning for iBeacons
  while (1)
  {
  	  evt_le_meta_event       *meta;                          									//- Used to determine the subevent code
      le_advertising_info *info;
      char addr[18];

      while ((len = read(dd, buf, sizeof(buf))) < 0)
      {
        if (errno == EINTR && signal_received == SIGINT)
        {
            len = 0;
            goto done;
        }

        if (errno == EAGAIN || errno == EINTR)												//- EAGAIN || EINTR - error signals for reading for a empty socket, means there is nothing available currently. so we try again. 
        	continue;
        goto done;
      }
      time_t now = time (0);																			//- Logging the current time here.

      ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
      len -= (1 + HCI_EVENT_HDR_SIZE);

      meta = (void *) ptr;

      if (meta->subevent != 0x02)             													//-Checking it conforms to the ADV_IND
          goto done;

      /* Ignoring multiple reports */
      info = (le_advertising_info *) (meta->data + 1);
      ba2str(&info->bdaddr, addr);            													//- Function located in bluetooth.h, converts it to a a char* e.g string
      if (info->length == 30)                         												//- Double Checking it is an iBeacon packet
      {
          strftime (buff, 100, "%Y-%m-%d %H:%M:%S", localtime (&now));			//- Converting the timestamp to a string
          printf("%s, %s, ", hostname, buff);
          for (i = 9; i < 25; i++) 							//- info->data[]9 to 24 is the uuid
          {
              printf("%02X",info->data[i]);
          }
          //- prefered format as it sticks to the convention defined where RSSI is the last value in the data
          //printf(", %d, %d\n",( info->data[29] - 256),( info->data[30] - 256) );      //- format- 1st %d = Calibrated Power/rssi at 1 m, 2nd % d = rssi - current received signal strength.
          //- Tommy's current setup, just to make life easy for him.
           printf(", %d, %d\n",( info->data[30] - 256),( info->data[29] - 256) );       //- format- 1st %d = rssi - current received signal strength, 2nd %d = Calibrated Power/rssi at 1m .
		 
	  //- BELOW IS USED FOR DEBUGING/VERIFYING PACKET DATA, UNCOMMENT TO SEE THE RAW DUMP
       	  /*printf("\nComplete Packet Length: %d\nComplete Packet: ",len);
          for (i = 0; i < len; i++)
          {
              printf("%02X",ptr[i]);
          }
          printf("\n"); */
		  
	  }
  }       //- End of While Loop

done:
        setsockopt(dd, SOL_HCI, HCI_FILTER, &of, sizeof(of));

        if (len < 0) return -1;
        
        return 0;
}

int main(int argc, char **argv)
{
    int dev_id, sock;
    int i,ctl, err;
    setvbuf(stdout, (char *) NULL, _IOLBF, 0);		//- Setting the stdout to line buffered, so it forces a flush on every \n (newline), so the python code reading the output does not have a buffer issue
    
    /* Open HCI socket  */
    if ((ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI)) < 0) 
    {
      perror("Can't open HCI socket.");
      exit(1);
    }
    /*Connecting to the device and attempting to get the devices info, if this errors, it implies the usb is not plugged in correctly or we may have a bad/dodgy bluetooth usb */
    if (ioctl(ctl, HCIGETDEVINFO, (void *) &di)) 
    {
      perror("Can't get device info: Make sure the bluetooth usb is properly inserted. ");
      exit(1);
    }
    
    dev_id = di.dev_id;         //- Assigning the devices id

    /* Stop HCI device - e.g the bluetooth usb - we are doing this to reset the adapter*/
    if (ioctl(ctl, HCIDEVDOWN, dev_id) < 0)
    {
        fprintf(stderr, "Can't down device hci%d: %s (%d)\n", dev_id, strerror(errno), errno);
        exit(1);
    }

    /* Start HCI device - e.g the bluetooth usb */
    if (ioctl(ctl, HCIDEVUP,dev_id) < 0)
    {
        if (errno == EALREADY) return;
        fprintf(stderr, "Can't init device hci%d: %s (%d)\n",dev_id, strerror(errno), errno);
        exit(1);
    }
	
    sock = hci_open_dev( dev_id );     			 //- Opening the ble device adapter so we can start scanning for iBeacons 

    /* checking for error whilst opening the socket */
    if (dev_id < 0 || sock < 0)
    {
        perror("opening socket");
        exit(1);
    }
    /* Setting the scan parameters */
    err = hci_le_set_scan_parameters(sock, /*scan_type*/0x00, /*interval*/htobs(0x0010), /*window*/htobs(0x0010), /*own_type*/LE_PUBLIC_ADDRESS, /*filter_policy*/0x00, 10000);
    if (err < 0) 
    {
        perror("Set scan parameters failed");
        exit(1);
    }
    /* hci bluetooth library call - enabling the scan */
    err = hci_le_set_scan_enable(sock, 0x01, /*filter_dup */0x01 , 10000);
    if (err < 0) 
    {
        perror("Enable scan failed");
        exit(1);
    }
    /* Our own print funtion based off the bluez print function */
    err = print_advertising_ble_beacons(sock, /*filter_type*/ 0);
    if (err < 0)
    {
        perror("Could not receive advertising events");
        exit(1);
    }
    /* hci bluetooth library call - disabling the scan */
    err = hci_le_set_scan_enable(sock, 0x00,/*filter_dup */0x01, 10000);
    if (err < 0) 
    {
        perror("Disable scan failed");
        exit(1);
    }

    hci_close_dev(sock);
    return 0;
}
