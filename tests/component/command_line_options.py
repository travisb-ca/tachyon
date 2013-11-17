#!/usr/bin/env python2.7

import subprocess

class TestCommandLineOptions(LousyTestCase):
	def setUp(self):
		self.tachyon = None
		self.tachyonTerminated = False

	def tearDown(self):
		if self.tachyon and not self.tachyonTerminated:
			self.tachyon.kill()

	def waitForTermination(self):
		while not self.tachyon.poll():
			pass
		self.tachyonTerminated = True

	def test_invalidOption(self):
		self.tachyon = subprocess.Popen('./tachyon -z', shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

		self.waitForTermination()

		self.assertEqual(self.tachyon.returncode, 1)

