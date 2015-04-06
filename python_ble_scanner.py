#   python_ble_scanner - Sends BLE packet data to server via websockets to a node/socket.io server.
#   Copyright (C) 2014  Charles Nelson
#	
#	************************************NOTE***********************************
#	NOTE: SOCKET.IO Code & comments are carljm work which can be found at 
#		https://gist.github.com/carljm/3d4873e763b279e26de4 
#		https://github.com/invisibleroads/socketIO-client/issues/52
#
#	Many thanks to carl
#	***************************************************************************
#   
#	This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.

#!/usr/bin/python
import platform
import subprocess
import re
import sys
#socket.io imports below     
import json
 
import requests
from websocket import create_connection

base_url = 'speedy-fireball-85-177924.apse2.nitrousbox.com:3000' #add the server's IP address and Port Number HERE
url = base_url + '/socket.io/'

BASE = url + '?EIO=3'
 
# First establish a polling-transport HTTP connection to get a session ID
url = 'http://%s&transport=polling' % BASE
resp = requests.get(url)
print resp.status_code, resp.headers, resp.content
 
# Ignore the bin-packed preliminaries in the SocketIO response and just parse
# the JSON; it tells us everything we really need to know
json_data = json.loads(resp.content[resp.content.find('{'):])
sid = json_data['sid']
assert sid == resp.cookies['io']
 
# Now we have a session ID, establish a websocket connection
ws_url = 'ws://%s&transport=websocket&sid=%s' % (BASE, sid)
conn = create_connection(ws_url)
 
# This is an "upgrade" packet. Not sure why it's needed, since we've already
# upgraded the connection to WebSocket and established a WebSocket
# connection. I guess maybe it upgrades our Socket.IO session to use this
# websocket connection as its transport? In any case, the next step doesn't
# work unless we do this first.
conn.send('5')						
 
# Send a socket.io event!
# 4 = MESSAGE
# 2 = EVENT
# "location" is event name, "%s"/heypacket_json is payload
popen = subprocess.Popen(["/home/pi/ble_raspi_scanner"], stdout=subprocess.PIPE)
for inputLine in iter(popen.stdout.readline, ""):
	packetString = inputLine
	heypacket_json = { "data": packetString }
	packet = '42["location",%s]' % json.dumps(heypacket_json)
	conn.send(packet)
 
#from IPython import embed; embed()
