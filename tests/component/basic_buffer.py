#!/usr/bin/env python2.7

import tachyon

class TestBasicWindows(tachyon.TachyonTestCase):
	def setUp1(self):
		self.startTachyon()

	def tearDown1(self):
		self.waitForTermination()

	def test_exitImmedately(self):
		self.sendLine('exit')

	def test_bufferShellResponds(self):
		self.sendCmd('export BUFNUM=1')
		self.sendCmd('echo $BUFNUM')
		self.expectOnly('^1$')
		self.sendCmd('exit')

	def test_createNewBuffer(self):
		self.sendCmd('export BUFNUM=1')
		self.sendMeta('c')
		self.sendCmd('export BUFNUM=2')
		self.sendCmd('echo $BUFNUM')
		self.expectOnly('^2$')
		self.sendCmd('exit')
		self.sendCmd('echo $BUFNUM')
		self.expectOnly('^1$')
		self.sendCmd('exit')
