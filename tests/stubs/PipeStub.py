#!/usr/bin/env python2.7
# 
# Copyright (C) 2014  Travis Brown (travisb@travisbrown.ca)
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# version 2 as published by the Free Software Foundation.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

# Simple stub which passes strings back and forth
# Currently this only takes strings from the test and prints them

import sys
import socket
import struct
import os

HEADER_FMT = '!L'

def send(sock, msg):
	header = struct.pack(HEADER_FMT, len(msg))
	sock.send(header + msg)

def recv(sock):
	size = sock.recv(4)
	if size == '':
		# Socket closed
		print '\nSocket closing, exiting'
		sys.exit(0)
	size = struct.unpack(HEADER_FMT, size)[0]

	msg = sock.recv(size)

	return msg

if len(sys.argv) < 2:
	print 'Usage: PipeStub.py port'
	sys.exit(1)

port = int(sys.argv[1])

sock = socket.create_connection(('localhost', port))
send(sock, 'PipeStub,%d' % os.getpid())

while True:
	msg = recv(sock)
	if msg != '':
		sys.stdout.write(msg)
		sys.stdout.flush()
