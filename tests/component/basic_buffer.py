#!/usr/bin/env python2.7

import tachyon

class TestBasicWindows(tachyon.TachyonTestCase):
	def setUp(self):
		self.startTachyon('-p')

	def tearDown(self):
		self.waitForTermination()

	def test_createNewBuffer(self):
		self.sendString('export BUFNUM=1')
		self.sendMeta('c')
		self.sendString('echo $BUFNUM')
		response = self.terminalLine(-1)
		self.assertEqual(response, '')

