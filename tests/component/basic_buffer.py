#!/usr/bin/env python2.7

import tachyon

class TestBasicWindows(tachyon.TachyonTestCase):
	def setUp(self):
		self.startTachyon()

	def tearDown(self):
		self.waitForTermination()

	def test_exitImmedately(self):
		self.sendLine('exit')

	def test_bufferShellResponds(self):
		self.sendLine('export BUFNUM=1')
		self.sendLine('echo $BUFNUM')
		response = self.terminalLine(-1)
		self.assertEqual(response, '1')
		self.sendLine('exit')

