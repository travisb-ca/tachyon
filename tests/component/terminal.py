#!/usr/bin/env python2.7

import tachyon

class TestTerminal(tachyon.TachyonTestCase):
	def setUp1(self):
		self.startTachyon()

	def tearDown1(self):
		self.waitForTermination()

	def test_bufferContentsRestoredOnSwitch(self):
		self.sendCmd('ls')
		before = self.snapShot()

		self.bufferCreate()
		self.sendCmd('printenv')

		self.bufferNext()

		after = self.snapShot()
		self.assertEqual(before, after)

		self.sendCmdExit()
		self.sendCmdExit()
