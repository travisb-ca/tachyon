#!/usr/bin/env python2.7

import tachyon

class TestCommandLineOptions(tachyon.TachyonTestCase):
	def setUp(self):
		pass

	def tearDown(self):
		pass

	def test_invalidOption(self):
		self.startTachyon('-z')
		self.waitForTermination()
		self.assertEqual(self.tachyon.returncode, 1)

