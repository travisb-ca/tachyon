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
		self.sendCmd('export BUFNUM=1')
		self.sendCmd('echo $BUFNUM')
		self.expectOnly('^1$')
		self.sendCmd('exit')
