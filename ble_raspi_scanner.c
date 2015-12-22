/*
    ble_raspi_scanner - Bluetooth low energy scanning of low energy advertisement packets, for capturing and displaying iBeacon data packets.
   	Copyright (C) 2014  Charles Nelson
	
   	********************************************NOTE********************************************
    Code is based/re-factored from and uses the Bluez - Bluetooth protocol stack for Linux. 
    
    You can find out more at - http://www.bluez.org/
    
    Contributors of the original source code used are: 
      Copyright (C) 2012  Intel Corporation. All rights reserved. 	
      Copyright (C) 2000-2001  Qualcomm Incorporated
      Copyright (C) 2002-2003  Maxim Krasnyansky <maxk@qualcomm.com>
      Copyright (C) 2002-2010  Marcel Holtmann <marcel@holtmann.org>
    *********************************************************************************************
    
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

/* Stores our ble adapter's device info. e.g- our address to use as a handle */
static struct hci_dev_info di; 
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
  /*  Used to retrieve the host name */
  char hostname[128];     
  /* Used for storing the time-stamp */
  char buff[100];						 
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
  
  /* Retrieving the hostname of the device that is scanning for the iBeacon packets. */
  gethostname(hostname, sizeof hostname); 
  while (1)
  {
    /* Used to determine the subevent code. */
    evt_le_meta_event   *meta;
    le_advertising_info *info;
    char addr[18];

    while ((len = read(dd, buf, sizeof(buf))) < 0)
    {
      if (errno == EINTR && signal_received == SIGINT)
      {
        len = 0;
        goto done;
      }
      /* Error signals for detecting an empty socket, 
         implies there is nothing available to currently read. 
         So we try again until there is.  */
      if (errno == EAGAIN || errno == EINTR)
      	continue;
      goto done;
    }
    /* Logging the current time. */
    time_t now = time (0);

    ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
    len -= (1 + HCI_EVENT_HDR_SIZE);

    meta = (void *) ptr;
    
    /* Checking if the data conforms to the ADV_IND specification. */
    if (meta->subevent != 0x02)
      goto done;

    /* Ignoring multiple reports. */
    info = (le_advertising_info *) (meta->data + 1);
    
    /* Note: Function can be located in bluetooth.h, converts data to a char* e.g string. */
    ba2str(&info->bdaddr, addr);
    
    /* Double checking if the data is an iBeacon packet */
    if (info->length == 30)
    {
      /* Converting the timestamp to a string. */
      strftime (buff, 100, "%Y-%m-%d %H:%M:%S", localtime (&now));
      printf("%s, %s, ", hostname, buff);
      /* NOTE: info->data[] from i = 9 to 24 is the uuid. */
      for (i = 9; i < 25; i++)
      {
        printf("%02X",info->data[i]);
      }
      /* Suggested & Prefered output format displayed below. 
         Below format is prefered as it sticks to the convention defined where 
         RSSI is the last value in the data.
         Format- 1st %d = Calibrated Power/rssi at 1 m, 2nd % d = rssi - current received signal strength.
         //printf(", %d, %d\n",( info->data[29] - 256),( info->data[30] - 256) );
      */
      
      /* Current output format is only in use to meet Tommy's current setup, makes life easier for him. 
         Format- 1st %d = rssi - current received signal strength, 2nd %d = Calibrated Power/rssi at 1m .
      */
      printf(", %d, %d\n",( info->data[30] - 256),( info->data[29] - 256) );       
      
      /* UNCOMMENT CODE BELOW SEE THE RAW DUMP, 
         ONLY TO BE USED FOR DEBUGING/VERIFYING PACKET DATA. */
   	  /*printf("\nComplete Packet Length: %d\nComplete Packet: ",len);
      for (i = 0; i < len; i++)
      {
        printf("%02X",ptr[i]);
      }
      printf("\n"); 
      */
	  }
  } //- End of While Loop

done:
  setsockopt(dd, SOL_HCI, HCI_FILTER, &of, sizeof(of));

  if (len < 0) return -1;
  
  return 0;
}

int main(int argc, char **argv)
{
  int dev_id, sock;
  int i,ctl, err;
  
  /* Setting the stdout to line buffered, this forces a flush on every '\n' (newline). 
     This will ensure that the python program consuming the output will not have any buffer issues. */
  setvbuf(stdout, (char *) NULL, _IOLBF, 0);
  
  /* Opening a HCI socket  */
  if ((ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI)) < 0) 
  {
    perror("Can't open HCI socket.");
    exit(1);
  }
  /* Connecting to the device and attempting to get the devices info, if this errors, 
     it implies the usb is not plugged in correctly or we may have a bad/dodgy bluetooth usb */
  if (ioctl(ctl, HCIGETDEVINFO, (void *) &di)) 
  {
    perror("Can't get device info: Make sure the bluetooth usb is properly inserted. ");
    exit(1);
  }
  
  /* Assigning the devices id */
  dev_id = di.dev_id;

  /* Stop HCI device (e.g - bluetooth usb) - we are doing this to reset the adapter */
  if (ioctl(ctl, HCIDEVDOWN, dev_id) < 0)
  {
    fprintf(stderr, "Can't down device hci%d: %s (%d)\n", dev_id, strerror(errno), errno);
    exit(1);
  }

  /* Start HCI device (e.g - bluetooth usb)*/
  if (ioctl(ctl, HCIDEVUP,dev_id) < 0)
  {
    if (errno == EALREADY) return;
    fprintf(stderr, "Can't init device hci%d: %s (%d)\n",dev_id, strerror(errno), errno);
    exit(1);
  }
  /* Opening the ble device adapter so we can start scanning for iBeacons  */
  sock = hci_open_dev( dev_id );

  /* Ensure that no error occured whilst opening the socket */
  if (dev_id < 0 || sock < 0)
  {
    perror("opening socket");
    exit(1);
  }
  
  /* Setting the scan parameters */
  err = hci_le_set_scan_parameters(sock, /*scan_type*/ 0x00, /*interval*/ htobs(0x0010), /*window*/ htobs(0x0010), /*own_type*/ LE_PUBLIC_ADDRESS, /*filter_policy*/ 0x00, 10000);
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
